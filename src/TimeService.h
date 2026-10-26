#pragma once
#include <stdint.h>
class TimeService {
 public:
  void begin();
  void tick(uint32_t now);
  bool valid() const { return valid_; }
  bool rtcHealthy() const { return healthy_; }
  int64_t utc() const { return seconds_; }
  uint32_t lastRtcMs = 0;
 private:
  bool read(int64_t& value);
  bool valid_ = false, healthy_ = false, first_ = true;
  int64_t seconds_ = 0;
  uint32_t advancedMs_ = 0, attemptedMs_ = 0;
};
