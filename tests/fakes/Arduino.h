#pragma once
#include <stdint.h>
inline uint32_t fakeMillis = 0;
inline uint32_t millis() { return fakeMillis; }
constexpr int LOW = 0, HIGH = 1, OUTPUT = 1;
inline int fakePin = -1, fakeLevel = -1, fakeModePin = -1;
inline unsigned fakeWrites = 0;
inline void digitalWrite(int pin, int level) { fakePin=pin; fakeLevel=level; ++fakeWrites; }
inline void pinMode(int pin, int) { fakeModePin=pin; }
