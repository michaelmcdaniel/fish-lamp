#include "TimeService.h"
#include "ClockMath.h"
#include "Devices.h"
#include "MockDebug.h"
#include <Arduino.h>
#include <Wire.h>

void TimeService::begin() {
  if(!devices::rtc) {
    seconds_=devices::mockRtcUtc; valid_=true; healthy_=false;
    advancedMs_=millis(); first_=false;
    mockdebug::log("mock RTC: boot UTC=%lld\r\n",static_cast<long long>(seconds_));
    return;
  }
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
bool TimeService::setUtc(int64_t utc, uint32_t now) {
  if (utc < clockmath::midnight({2000,1,1}) || utc >= clockmath::midnight({2100,1,1})) return false;
  if(!devices::rtc) {
    seconds_=utc; advancedMs_=now; valid_=true; healthy_=false; first_=false;
    mockdebug::log("mock RTC: set UTC=%lld (RAM only)\r\n",static_cast<long long>(utc));
    return true;
  }
  auto date = clockmath::dateAt(utc);
  auto bcd = [](int value) { return uint8_t((value/10)*16 + value%10); };
  uint8_t status;
  if (!registers(0x0f, &status, 1)) return false;
  Wire.beginTransmission(0x68);
  Wire.write(0);
  Wire.write(bcd(int(utc % 60)));
  Wire.write(bcd(int(utc / 60 % 60)));
  Wire.write(bcd(int(utc / 3600 % 24)));
  Wire.write(uint8_t((utc / 86400 + 4) % 7 + 1));
  Wire.write(bcd(date.day)); Wire.write(bcd(date.month)); Wire.write(bcd(date.year-2000));
  if (Wire.endTransmission() != 0) return false;
  Wire.beginTransmission(0x68);
  Wire.write(0x0f); Wire.write(uint8_t(status & ~0x80));
  if (Wire.endTransmission() != 0) return false;
  int64_t check;
  if (!read(check) || check < utc || check > utc + 1) return false;
  seconds_ = check; advancedMs_ = attemptedMs_ = lastRtcMs = now;
  valid_ = healthy_ = true; first_ = false;
  return true;
}
void TimeService::tick(uint32_t now) {
  uint32_t elapsed = (now - advancedMs_) / 1000;
  if (valid_) seconds_ += elapsed;
  advancedMs_ += elapsed * 1000;
  if(!devices::rtc) return;
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
