#include "Diagnostics.h"
#include <Arduino.h>
#include <cstdio>
#include <algorithm>

void Diagnostics::begin() { resetReason_ = int(rp2040.getResetReason()); }
void Diagnostics::tick(uint32_t now, uint32_t loopUs, const ConfigManager& config,
                       const TimeService& time, ScheduleController& schedule,
                       const LightingController& lights) {
  maxLoopUs_ = std::max(maxLoopUs_, loopUs);
  // Process at most one byte and transmit at most 32 bytes per loop. USB host
  // absence/backpressure must never prevent essential control progress.
  if (Serial.available() > 0) {
    int command = Serial.read();
    if (command == 's') first_ = true;
    if (config.valid() && time.valid()) {
      if (command == 'l' || command == 'L')
        schedule.lightsOverride.request(command == 'l', command == 'L', now, schedule.nextLightsBoundary);
      if (command == 'g' || command == 'G')
        schedule.growOverride.request(command == 'g', false, now, schedule.nextGrowBoundary);
      if (command == 'a') {
        schedule.lightsOverride.active = false;
        schedule.growOverride.active = false;
      }
    }
  }
  if (sent_ == length_ && (first_ || uint32_t(now-lastMs_) >= 30000)) {
    first_ = false; lastMs_ = now;
    int n = snprintf(output_, sizeof(output_),
      "config=%d time=%d rtc=%d solar=%d day=%d grow=%d forcedOff=%d strip=%d "
      "rtcAgeMs=%lu scheduleAgeMs=%lu applyAgeMs=%lu maxControlUs=%lu reset=%d\r\n",
      int(config.state), time.valid(), time.rtcHealthy(), schedule.solarValid,
      schedule.dayOn, schedule.growOn, schedule.forceOff, lights.ready(),
      static_cast<unsigned long>(now-time.lastRtcMs),
      static_cast<unsigned long>(now-schedule.lastCalculatedMs),
      static_cast<unsigned long>(now-lights.lastAppliedMs),
      static_cast<unsigned long>(maxLoopUs_), resetReason_);
    length_ = n > 0 ? std::min(unsigned(n), unsigned(sizeof(output_)-1)) : 0;
    sent_ = 0;
  }
  if (Serial && sent_ < length_) {
    unsigned room = unsigned(std::max(0, Serial.availableForWrite()));
    unsigned count = std::min(32u, std::min(room, length_-sent_));
    if (count) sent_ += Serial.write(reinterpret_cast<const uint8_t*>(output_+sent_), count);
  }
}
