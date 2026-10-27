#pragma once
#include "WiFi.h"
inline unsigned fakeDnsStarts=0, fakeDnsStops=0, fakeDnsRequests=0;
inline bool fakeDnsOK=true;
class DNSServer {
 public:
  void setTTL(unsigned) {}
  bool start(unsigned port,const char* name,IPAddress) {
    ++fakeDnsStarts;
    return fakeDnsOK && port==53 && !strcmp(name,"*");
  }
  void stop() { ++fakeDnsStops; }
  void processNextRequest() { ++fakeDnsRequests; }
};
