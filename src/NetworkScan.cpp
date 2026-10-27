#include "NetworkScan.h"
#include "MockDebug.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

static bool utf8(const uint8_t* data,unsigned length) {
  for(unsigned i=0;i<length;) {
    unsigned char first=data[i++];
    if(!first) return false; // Binary SSIDs cannot be entered through this UI.
    if(first<0x80) continue;
    unsigned n=first>=0xc2 && first<=0xdf ? 1 : first>=0xe0 && first<=0xef ? 2 : first>=0xf0 && first<=0xf4 ? 3 : 4;
    if(n==4 || i+n>length) return false;
    unsigned char second=data[i];
    if((first==0xe0 && second<0xa0) || (first==0xed && second>=0xa0) ||
       (first==0xf0 && second<0x90) || (first==0xf4 && second>=0x90)) return false;
    while(n--) if((data[i++]&0xc0)!=0x80) return false;
  }
  return true;
}
int NetworkScan::result(void* context,const cyw43_ev_scan_result_t* network) {
  auto& scan=*static_cast<NetworkScan*>(context);
  // Callback runs under CYW43's lock. No allocations, logging, or HTTP here.
  if(!scan.collecting_ || !network || !network->ssid_len || network->ssid_len>32 ||
     !utf8(network->ssid,network->ssid_len)) return 0;
  char name[33]{}; memcpy(name,network->ssid,network->ssid_len);
  for(unsigned i=0;i<scan.count_;++i) if(!strcmp(name,scan.networks_[i].ssid)) {
    scan.networks_[i].rssi=std::max(scan.networks_[i].rssi,int(network->rssi)); return 0;
  }
  unsigned slot=scan.count_;
  if(slot<Capacity) ++scan.count_;
  else {
    slot=0;
    for(unsigned i=1;i<Capacity;++i) if(scan.networks_[i].rssi<scan.networks_[slot].rssi) slot=i;
    if(network->rssi<=scan.networks_[slot].rssi) return 0;
  }
  memcpy(scan.networks_[slot].ssid,name,sizeof(name)); scan.networks_[slot].rssi=network->rssi;
  return 0;
}
void NetworkScan::begin(uint32_t now) {
  cyw43_arch_enable_sta_mode();
  cyw43_arch_lwip_begin();
  count_=0; done_=false; collecting_=true; started_=now;
  cyw43_wifi_scan_options_t options{};
  startError_=cyw43_wifi_scan(&cyw43_state,&options,this,result);
  failed_=startError_!=0;
  cyw43_arch_lwip_end();
}
bool NetworkScan::tick(uint32_t now) {
  if(done_) return true;
  cyw43_arch_lwip_begin();
  bool active=cyw43_wifi_scan_active(&cyw43_state);
  bool timeout=uint32_t(now-started_)>=8000;
  bool finished=failed_ || !active || timeout;
  if(finished) collecting_=false; // Ignore late callbacks after timeout.
  cyw43_arch_lwip_end();
  if(!finished) return false;
  cyw43_arch_disable_sta_mode();
  const char* status=failed_ ? "failed" : active && timeout ? "timeout" : "ready";
  serialize(status);
  mockdebug::log("WiFi scan: %s, count=%u, error=%d\r\n",status,count_,startError_);
  done_=true;
  return true;
}
void NetworkScan::serialize(const char* status) {
  std::sort(networks_,networks_+count_,[](const Network& a,const Network& b){
    return a.rssi!=b.rssi ? a.rssi>b.rssi : strcmp(a.ssid,b.ssid)<0;
  });
  size_t used=size_t(snprintf(json_,sizeof(json_),"{\"status\":\"%s\",\"count\":%u,\"error\":%d,\"networks\":[",status,count_,startError_));
  constexpr char hex[]="0123456789abcdef";
  for(unsigned i=0;i<count_;++i) {
    if(i)json_[used++]=',';
    json_[used++]='"';
    for(const unsigned char* p=reinterpret_cast<const unsigned char*>(networks_[i].ssid);*p;++p) {
      if(*p=='"' || *p=='\\') {json_[used++]='\\';json_[used++]=char(*p);}
      else if(*p<32) {
        memcpy(json_+used,"\\u00",4);used+=4;json_[used++]=hex[*p>>4];json_[used++]=hex[*p&15];
      } else json_[used++]=char(*p);
    }
    json_[used++]='"';
  }
  memcpy(json_+used,"]}",3);
}
