#line 1 "C:\\Users\\mmcda\\OneDrive\\Code\\pico\\fish-lamp\\src\\Diagnostics.cpp"
#include "Diagnostics.h"
#include "MockDebug.h"
#include <Arduino.h>
#include <cstdio>
#include <algorithm>

void Diagnostics::begin() { resetReason_ = int(rp2040.getResetReason()); }
void Diagnostics::tick(uint32_t now, uint32_t loopUs, const ConfigManager& config,
                       const TimeService& time, ScheduleController& schedule,
                       const LightingController& lights, int networkState, const SensorManager& sensors) {
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
      "config=%d network=%d time=%d rtc=%d solar=%d day=%d grow=%d forcedOff=%d strip=%d "
      "rtcAgeMs=%lu scheduleAgeMs=%lu applyAgeMs=%lu maxControlUs=%lu reset=%d "
      "realDevices(strip,grow,rtc,water,ambient,cpu,status,wifi)=%d%d%d%d%d%d%d%d "
      "waterF=%.2f(valid=%d,healthy=%d) ambientPct=%.1f(valid=%d,healthy=%d) "
      "cpuC=%.2f(valid=%d,healthy=%d) mockDropped=%u\r\n",
      int(config.state), networkState, time.valid(), time.rtcHealthy(), schedule.solarValid,
      schedule.dayOn, schedule.growOn, schedule.forceOff, lights.ready(),
      static_cast<unsigned long>(now-time.lastRtcMs),
      static_cast<unsigned long>(now-schedule.lastCalculatedMs),
      static_cast<unsigned long>(now-lights.lastAppliedMs),
      static_cast<unsigned long>(maxLoopUs_), resetReason_,
      devices::strip,devices::grow,devices::rtc,devices::water,devices::ambient,devices::cpu,devices::status,devices::wifi,
      double(sensors.waterF.value),sensors.waterF.valid,sensors.waterF.healthy,
      double(sensors.ambientPercent.value),sensors.ambientPercent.valid,sensors.ambientPercent.healthy,
      double(sensors.cpuC.value),sensors.cpuC.valid,sensors.cpuC.healthy,mockdebug::dropped());
    length_ = n > 0 ? std::min(unsigned(n), unsigned(sizeof(output_)-1)) : 0;
    sent_ = 0;
  }
  if(sent_==length_) {length_=unsigned(mockdebug::take(output_,sizeof(output_)));sent_=0;}
  if (Serial && sent_ < length_) {
    unsigned room = unsigned(std::max(0, Serial.availableForWrite()));
    unsigned count = std::min(32u, std::min(room, length_-sent_));
    if (count) sent_ += Serial.write(reinterpret_cast<const uint8_t*>(output_+sent_), count);
  }
}
