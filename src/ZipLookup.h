#pragma once
#include <lwip/tcp.h>
#include <cstddef>
#include <cstdint>

// One asynchronous, six-second setup-only lookup. No retries or lighting I/O.
class ZipLookup {
 public:
  ZipLookup()=default;
  ZipLookup(const ZipLookup&)=delete;
  ZipLookup& operator=(const ZipLookup&)=delete;
  ~ZipLookup();
  void begin(const char* zip,uint32_t now);
  bool tick(uint32_t now);
  void cancel();
  bool found() const { return found_; }
  double latitude() const { return latitude_; }
  double longitude() const { return longitude_; }
  static bool parseResponse(char* response,size_t length,double& latitude,double& longitude);
 private:
  static void resolved(const char*,const ip_addr_t*,void*);
  static err_t connected(void*,tcp_pcb*,err_t);
  static err_t received(void*,tcp_pcb*,pbuf*,err_t);
  static void error(void*,err_t);
  void close(); // Caller holds the CYW43/lwIP lock.
  enum class Phase { Idle, DNS, Connect, Read, Complete, Failed, Done };
  Phase phase_=Phase::Idle;
  tcp_pcb* pcb_=nullptr;
  ip_addr_t address_{};
  bool addressReady_=false,found_=false;
  uint32_t started_=0;
  double latitude_=0,longitude_=0;
  size_t length_=0;
  char zip_[6]{},response_[4097]{};
};
