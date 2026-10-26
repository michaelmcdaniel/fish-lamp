#pragma once
#include "Config.h"
enum class ConfigState { MountFailed, Invalid, Primary, Backup };
class ConfigManager {
 public:
  void begin();
  bool save(const char* json, size_t length);
  bool valid() const { return state == ConfigState::Primary || state == ConfigState::Backup; }
  Config config;
  ConfigState state = ConfigState::MountFailed;
 private:
  bool mounted_ = false;
};
