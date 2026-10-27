#pragma once
#include "Config.h"
class ScheduleController {
 public:
  void tick(int64_t utc, uint32_t ms, const Config& config);
  bool dayOn = false, growOn = false, forceOff = false, solarValid = false;
  ManualOverride lightsOverride, growOverride;
  uint32_t lastCalculatedMs = 0;
  int64_t nextLightsBoundary = 0, nextGrowBoundary = 0;
  // Union transitions across cached adjacent days, before manual overrides.
  void nextTransitions(int64_t utc, bool grow, int64_t& on, int64_t& off) const;
 private:
  int64_t localDay_ = -1;
  ScheduleDay days_[3]{};
};
