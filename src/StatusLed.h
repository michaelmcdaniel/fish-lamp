#pragma once
class StatusLed {
 public:
  void begin();
  void apply(bool on);
 private:
  bool on_=false;
};
