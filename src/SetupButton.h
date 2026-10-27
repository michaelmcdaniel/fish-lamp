#pragma once
#include <stdint.h>
class ConfigManager;

class SetupButton {
 public:
  // Two consecutive pressed samples debounce the button before arming.
  static constexpr uint32_t HoldMs=20, PollMs=20;
  void tick(uint32_t now,ConfigManager& settings);
 private:
  uint32_t sampledMs_=0, pressedMs_=0;
  bool pressed_=false, handled_=false, armed_=false;
};
