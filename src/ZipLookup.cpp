#include "ZipLookup.h"
#include "ZipCodes.h"
#include <pico/cyw43_arch.h>
#include <lwip/dns.h>
#include <ArduinoJson.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace {
// lwIP cannot cancel a DNS query. This slot outlives any ZipLookup object, and
// prevents a late callback from touching a destroyed object or a newer query.
struct { ZipLookup* owner=nullptr;bool busy=false; } dns;
class Arena : public ArduinoJson::Allocator {
  struct alignas(max_align_t) Header {size_t size;};
  // ArduinoJson's initial pool is larger on the 64-bit host test runner.
  alignas(max_align_t) unsigned char bytes_[sizeof(void*)>4 ? 8192 : 2048]{};
  size_t used_=0;
 public:
  void* allocate(size_t n) override {
    const size_t total=(n+sizeof(Header)+alignof(max_align_t)-1)/alignof(max_align_t)*alignof(max_align_t);
    if(total>sizeof(bytes_)-used_)return nullptr;
    auto* header=reinterpret_cast<Header*>(bytes_+used_);header->size=n;used_+=total;return header+1;
  }
  void deallocate(void*) override {}
  void* reallocate(void* p,size_t n) override {
    if(!p)return allocate(n);
    auto* header=static_cast<Header*>(p)-1;
    if(n<=header->size)return p; // ArduinoJson requires shrinking to succeed.
    void* next=allocate(n);if(next)memcpy(next,p,header->size);return next;
  }
};
bool coordinate(JsonVariantConst v,double limit,double& out) {
  if(v.is<double>())out=v.as<double>();
  else if(v.is<const char*>()) {
    const char* text=v.as<const char*>();char* end=nullptr;
    out=strtod(text,&end);if(end==text || *end)return false;
  } else return false;
  return std::isfinite(out) && std::abs(out)<=limit;
}
}
ZipLookup::~ZipLookup(){cancel();}
void ZipLookup::close() {
  if(dns.owner==this)dns.owner=nullptr;
  if(pcb_) {tcp_arg(pcb_,nullptr);tcp_recv(pcb_,nullptr);tcp_err(pcb_,nullptr);tcp_abort(pcb_);pcb_=nullptr;}
}
void ZipLookup::cancel() {
  cyw43_arch_lwip_begin();close();phase_=Phase::Idle;cyw43_arch_lwip_end();
}
void ZipLookup::begin(const char* zip,uint32_t now) {
  cancel();found_=false;length_=0;addressReady_=false;started_=now;
  if(!zipcodes::valid(zip)){phase_=Phase::Done;return;}
  memcpy(zip_,zip,6);
  cyw43_arch_lwip_begin();
  phase_=Phase::DNS;
  if(dns.busy)phase_=Phase::Failed;
  else {
    dns.owner=this;dns.busy=true;
    err_t result=dns_gethostbyname("api.zippopotam.us",&address_,resolved,nullptr);
    if(result!=ERR_INPROGRESS) {
      dns.owner=nullptr;dns.busy=false;
      if(result==ERR_OK)addressReady_=true;else phase_=Phase::Failed;
    }
  }
  cyw43_arch_lwip_end();
}
void ZipLookup::resolved(const char*,const ip_addr_t* address,void*) {
  if(dns.owner) {
    if(address){dns.owner->address_=*address;dns.owner->addressReady_=true;}
    else dns.owner->phase_=Phase::Failed;
  }
  dns.owner=nullptr;dns.busy=false;
}
err_t ZipLookup::connected(void* context,tcp_pcb* pcb,err_t result) {
  auto& lookup=*static_cast<ZipLookup*>(context);
  if(result!=ERR_OK){lookup.phase_=Phase::Failed;return ERR_OK;}
  char request[160];
  int length=snprintf(request,sizeof(request),"GET /us/%s HTTP/1.0\r\nHost: api.zippopotam.us\r\nConnection: close\r\nAccept: application/json\r\n\r\n",lookup.zip_);
  if(tcp_write(pcb,request,u16_t(length),TCP_WRITE_FLAG_COPY)!=ERR_OK)lookup.phase_=Phase::Failed;
  else {lookup.phase_=Phase::Read;tcp_output(pcb);}
  return ERR_OK;
}
err_t ZipLookup::received(void* context,tcp_pcb* pcb,pbuf* packet,err_t result) {
  auto& lookup=*static_cast<ZipLookup*>(context);
  if(!packet){if(lookup.phase_==Phase::Read)lookup.phase_=Phase::Complete;return ERR_OK;}
  if(lookup.phase_!=Phase::Read){pbuf_free(packet);return ERR_OK;}
  if(result!=ERR_OK || packet->tot_len>sizeof(lookup.response_)-1-lookup.length_) {
    pbuf_free(packet);lookup.phase_=Phase::Failed;return ERR_OK;
  }
  lookup.length_+=pbuf_copy_partial(packet,lookup.response_+lookup.length_,packet->tot_len,0);
  tcp_recved(pcb,packet->tot_len);pbuf_free(packet);
  return ERR_OK;
}
void ZipLookup::error(void* context,err_t) {
  auto& lookup=*static_cast<ZipLookup*>(context);lookup.pcb_=nullptr;lookup.phase_=Phase::Failed;
}
bool ZipLookup::tick(uint32_t now) {
  cyw43_arch_lwip_begin();
  if(phase_==Phase::DNS && addressReady_) {
    phase_=Phase::Connect;pcb_=tcp_new();
    if(!pcb_)phase_=Phase::Failed;
    else {
      tcp_arg(pcb_,this);tcp_recv(pcb_,received);tcp_err(pcb_,error);
      if(tcp_connect(pcb_,&address_,80,connected)!=ERR_OK)phase_=Phase::Failed;
    }
  }
  const bool parse=phase_==Phase::Complete;
  bool done=parse || phase_==Phase::Failed || phase_==Phase::Done || uint32_t(now-started_)>=6000;
  if(done){close();phase_=Phase::Done;}
  cyw43_arch_lwip_end();
  if(parse)found_=parseResponse(response_,length_,latitude_,longitude_);
  return done;
}
bool ZipLookup::parseResponse(char* response,size_t length,double& latitude,double& longitude) {
  if(!response || length>4096)return false;
  response[length]=0; // Caller supplies length+1 storage.
  if(strncmp(response,"HTTP/1.1 200 ",13) && strncmp(response,"HTTP/1.0 200 ",13))return false;
  char* body=strstr(response,"\r\n\r\n");if(!body)return false;body+=4;
  // HTTP/1.0 requests normally return a close-delimited body. Decode bounded
  // chunked replies too; no extensions/trailers are needed to read the JSON.
  for(char* p=response;p<body;++p)if(*p>='A' && *p<='Z')*p=char(*p+32);
  if(strstr(response,"transfer-encoding: chunked")) {
    char* source=body;char* end=response+length;char* dest=body;bool terminated=false;
    while(source<end) {
      char* numberEnd=nullptr;unsigned long count=strtoul(source,&numberEnd,16);
      if(numberEnd==source || size_t(end-numberEnd)<2 || strncmp(numberEnd,"\r\n",2))return false;
      source=numberEnd+2;
      if(!count){terminated=true;break;}
      if(count>size_t(end-source) || size_t(end-source)-count<2 || strncmp(source+count,"\r\n",2))return false;
      memmove(dest,source,count);dest+=count;source+=count+2;
    }
    if(!terminated)return false;
    *dest=0;length=size_t(dest-response);
  }
  Arena arena;JsonDocument doc(&arena);
  if(deserializeJson(doc,body,size_t(response+length-body),DeserializationOption::NestingLimit(4)))return false;
  auto place=doc["places"][0];double lat,lon;
  if(!coordinate(place["latitude"],90,lat) || !coordinate(place["longitude"],180,lon))return false;
  latitude=lat;longitude=lon;return true;
}
