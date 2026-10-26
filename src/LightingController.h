#pragma once
#include <Adafruit_NeoPixel.h>
#include <stdint.h>

#include "Config.h"

class LightingController {
 public:
  void begin();
  void apply(const Pixel* pixels, bool growOn);
  uint32_t lastAppliedMs = 0;
  bool ready() const { return ready_; }
 private:
  Adafruit_NeoPixel strip_{PixelCount, 7, NEO_GRB + NEO_KHZ800};
  bool ready_ = false;
  bool growOn_ = false;
};
