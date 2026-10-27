#pragma once
#include <stdint.h>
class StatusLed {
 public:
  void begin();
  void apply(bool on);
  void tick(uint32_t now,bool connecting,bool connected);
 private:
  bool on_=false;
};
