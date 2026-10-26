#pragma once
#include <stdint.h>
inline uint32_t fakeMillis = 0;
inline uint32_t millis() { return fakeMillis; }
constexpr int LOW = 0, HIGH = 1, OUTPUT = 1;
inline int fakePin = -1, fakeLevel = -1, fakeModePin = -1;
inline unsigned fakeWrites = 0;
inline void digitalWrite(int pin, int level) { fakePin=pin; fakeLevel=level; ++fakeWrites; }
inline void pinMode(int pin, int) { fakeModePin=pin; }
constexpr int LED_BUILTIN=64;
inline unsigned fakeAdcReads=0, fakeCpuReads=0;
inline int fakeAdc=2048;
inline float fakeCpu=30;
inline void analogReadResolution(int) {}
inline int analogRead(int) {++fakeAdcReads;return fakeAdc;}
inline float analogReadTemp() {++fakeCpuReads;return fakeCpu;}
struct FakeRp2040 {
  unsigned reboots = 0;
  uint32_t random = 0;
  uint32_t hwrand32() { return ++random; }
  void reboot() { ++reboots; }
};
inline FakeRp2040 rp2040;
