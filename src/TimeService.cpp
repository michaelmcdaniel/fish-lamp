#include "TimeService.h"
#include "ClockMath.h"
#include <Arduino.h>
#include <Wire.h>

void TimeService::begin() {
  Wire.setSDA(16);
  Wire.setSCL(17);
  Wire.begin();
  Wire.setClock(100000);
  Wire.setTimeout(5, true);
  advancedMs_ = millis();
}
static bool registers(uint8_t address, uint8_t* data, unsigned count) {
  Wire.beginTransmission(0x68);
  Wire.write(address);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(uint8_t(0x68), uint8_t(count)) != count) return false;
  for (unsigned i = 0; i < count; ++i) data[i] = static_cast<uint8_t>(Wire.read());
  return true;
}
bool TimeService::read(int64_t& value) {
  uint8_t data[7], status;
  return registers(0x0f, &status, 1) && registers(0, data, 7) &&
         clockmath::decodeRtc(data, status, value);
}
void TimeService::tick(uint32_t now) {
  uint32_t elapsed = (now - advancedMs_) / 1000;
  if (valid_) seconds_ += elapsed;
  advancedMs_ += elapsed * 1000;
  if (!first_ && uint32_t(now - attemptedMs_) < 60000) return;
  first_ = false;
  attemptedMs_ = now;
  int64_t candidate;
  healthy_ = read(candidate);
  if (healthy_) {
    seconds_ = candidate;
    advancedMs_ = now;
    valid_ = true;
    lastRtcMs = now;
  }
  // Failed reads preserve the validated software timebase, including millis wrap.
}
