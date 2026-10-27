#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <algorithm>
using err_t=int;using u16_t=uint16_t;
constexpr err_t ERR_OK=0,ERR_INPROGRESS=-5,ERR_CONN=-11;
constexpr int TCP_WRITE_FLAG_COPY=1;
struct ip_addr_t {uint32_t address=1;};
struct pbuf {u16_t tot_len;const char* payload;};
struct tcp_pcb {
  void* context=nullptr;
  err_t (*connected)(void*,tcp_pcb*,err_t)=nullptr;
  err_t (*recv)(void*,tcp_pcb*,pbuf*,err_t)=nullptr;
  void (*error)(void*,err_t)=nullptr;
  bool aborted=false;std::string request;
};
inline tcp_pcb fakeTcp;
inline err_t fakeTcpConnectResult=ERR_OK;
inline tcp_pcb* tcp_new(){fakeTcp=tcp_pcb{};return &fakeTcp;}
inline void tcp_arg(tcp_pcb* p,void* context){p->context=context;}
inline void tcp_recv(tcp_pcb* p,err_t (*fn)(void*,tcp_pcb*,pbuf*,err_t)){p->recv=fn;}
inline void tcp_err(tcp_pcb* p,void (*fn)(void*,err_t)){p->error=fn;}
inline void tcp_abort(tcp_pcb* p){p->aborted=true;}
inline err_t tcp_connect(tcp_pcb* p,const ip_addr_t*,int,err_t (*fn)(void*,tcp_pcb*,err_t)){p->connected=fn;return fakeTcpConnectResult;}
inline err_t tcp_write(tcp_pcb* p,const void* bytes,u16_t length,int){p->request.append(static_cast<const char*>(bytes),length);return ERR_OK;}
inline err_t tcp_output(tcp_pcb*){return ERR_OK;}
inline void tcp_recved(tcp_pcb*,u16_t){}
inline u16_t pbuf_copy_partial(pbuf* p,void* destination,u16_t length,u16_t offset){memcpy(destination,p->payload+offset,length);return length;}
inline void pbuf_free(pbuf*){}
inline void fakeTcpReply(const std::string& response) {
  fakeTcp.connected(fakeTcp.context,&fakeTcp,ERR_OK);
  // Fragment at arbitrary byte boundaries, just like TCP.
  for(size_t offset=0;offset<response.size();offset+=17) {
    pbuf packet{u16_t(std::min(size_t(17),response.size()-offset)),response.data()+offset};
    fakeTcp.recv(fakeTcp.context,&fakeTcp,&packet,ERR_OK);
  }
  fakeTcp.recv(fakeTcp.context,&fakeTcp,nullptr,ERR_OK);
}
