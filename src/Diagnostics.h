#pragma once
#include "ConfigManager.h"
#include "TimeService.h"
#include "ScheduleController.h"
#include "LightingController.h"

class Diagnostics {
 public:
  void begin();
  void tick(uint32_t now, uint32_t loopUs, const ConfigManager& config,
            const TimeService& time, ScheduleController& schedule,
            const LightingController& lights, int networkState);
 private:
  char output_[320]{};
  unsigned length_ = 0, sent_ = 0;
  uint32_t lastMs_ = 0, maxLoopUs_ = 0;
  bool first_ = true;
  int resetReason_ = 0;
};
