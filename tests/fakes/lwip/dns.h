#pragma once
#include "tcp.h"
inline err_t fakeDnsResult=ERR_OK;
inline void (*fakeDnsCallback)(const char*,const ip_addr_t*,void*)=nullptr;
inline void* fakeDnsContext=nullptr;
inline unsigned fakeZipDnsQueries=0;
inline err_t dns_gethostbyname(const char*,ip_addr_t* address,void (*fn)(const char*,const ip_addr_t*,void*),void* context) {
  ++fakeZipDnsQueries;fakeDnsCallback=fn;fakeDnsContext=context;*address=ip_addr_t{};return fakeDnsResult;
}
