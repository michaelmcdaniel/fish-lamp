#pragma once
#include <pico/cyw43_arch.h>
#include <stdint.h>

// Fixed storage avoids the installed WiFi.scanNetworks() implementation's
// unbounded std::map. Called on core 0, before the setup access point starts.
class NetworkScan {
 public:
  void begin(uint32_t now);
  bool tick(uint32_t now);
  const char* json() const { return json_; }
 private:
  static constexpr unsigned Capacity=24;
  struct Network { char ssid[33]{}; int rssi=0; };
  static int result(void* context,const cyw43_ev_scan_result_t* network);
  void serialize(const char* status);
  Network networks_[Capacity]{};
  unsigned count_=0;
  bool collecting_=false, done_=false, failed_=false;
  int startError_=0;
  uint32_t started_=0;
  // Worst case: every byte of every name needs six JSON escape characters.
  char json_[Capacity*(32*6+3)+96]{};
};
