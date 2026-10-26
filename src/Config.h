#pragma once
#include "SolarSchedule.h"
#include <stddef.h>
constexpr unsigned PixelCount = 85;
constexpr size_t MaxConfigBytes = 16384;
struct Pixel { uint8_t r, g, b, brightness; };
struct Config {
  Pixel lights[PixelCount]{}, nightlights[PixelCount]{};
  Offsets lightsOffsets, growOffsets;
  double latitude = 0, longitude = 0;
  clockmath::Zone zone;
};
// Failure never modifies destination. Credentials are not retained or logged.
bool parseConfig(const char* json, size_t length, Config& destination);
