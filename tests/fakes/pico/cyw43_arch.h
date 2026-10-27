#pragma once
#include <stdint.h>
struct cyw43_t {};
struct cyw43_wifi_scan_options_t {};
struct cyw43_ev_scan_result_t { int16_t rssi=0; uint8_t ssid_len=0; uint8_t ssid[32]{}; };
inline cyw43_t cyw43_state;
constexpr int CYW43_ITF_STA=0, CYW43_LINK_DOWN=0, CYW43_LINK_JOIN=1;
constexpr int CYW43_LINK_NONET=-2, CYW43_LINK_BADAUTH=-3;
struct FakeScan {
  bool active=false, station=false;
  int error=0;
  int linkStatus=CYW43_LINK_DOWN;
  unsigned starts=0, disables=0;
  void* context=nullptr;
  int (*callback)(void*,const cyw43_ev_scan_result_t*)=nullptr;
};
inline FakeScan fakeScan;
inline int cyw43_wifi_link_status(cyw43_t*,int) { return fakeScan.linkStatus; }
inline void cyw43_arch_enable_sta_mode() { fakeScan.station=true; }
inline void cyw43_arch_disable_sta_mode() { fakeScan.station=false;++fakeScan.disables; }
inline void cyw43_arch_lwip_begin() {}
inline void cyw43_arch_lwip_end() {}
inline int cyw43_wifi_scan(cyw43_t*,cyw43_wifi_scan_options_t*,void* context,int (*callback)(void*,const cyw43_ev_scan_result_t*)) {
  ++fakeScan.starts;fakeScan.context=context;fakeScan.callback=callback;return fakeScan.error;
}
inline bool cyw43_wifi_scan_active(cyw43_t*) { return fakeScan.active; }
