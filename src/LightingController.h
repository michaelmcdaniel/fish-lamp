#pragma once
#include <Adafruit_NeoPixel.h>
#include <stdint.h>

#include "Config.h"
#include "Devices.h"

class LightingController {
 public:
  void begin();
  void apply(const Pixel* pixels, bool growOn);
  uint32_t lastAppliedMs = 0;
  bool ready() const { return ready_; }
 private:
#if FISH_LAMP_STRIP
  Adafruit_NeoPixel strip_{PixelCount, 7, NEO_GRB + NEO_KHZ800};
#endif
  uint32_t applied_[PixelCount]{};
  bool ready_ = false;
  bool growOn_ = false;
};
