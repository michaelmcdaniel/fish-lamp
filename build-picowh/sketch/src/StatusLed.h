#line 1 "C:\\Users\\mmcda\\OneDrive\\Code\\pico\\fish-lamp\\src\\StatusLed.h"
#pragma once
class StatusLed {
 public:
  void begin();
  void apply(bool on);
 private:
  bool on_=false;
};
