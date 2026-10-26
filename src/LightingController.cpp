#include "LightingController.h"

void LightingController::begin() {
  digitalWrite(8, LOW);
  pinMode(8, OUTPUT);
  ready_ = strip_.begin();
  if (ready_) {
    strip_.clear();
    strip_.show();
    lastAppliedMs = millis();
  }
}

void LightingController::apply(const Pixel* pixels, bool growOn) {
  if (growOn_ != growOn) {
    digitalWrite(8, growOn ? HIGH : LOW);
    growOn_ = growOn;
    lastAppliedMs = millis();
  }
  if (!ready_) return;
  bool changed = false;
  for (unsigned i = 0; i < PixelCount; ++i) {
    const Pixel p = pixels ? pixels[i] : Pixel{0, 0, 0, 0};
    const uint32_t color = strip_.Color(
        (uint16_t(p.r) * p.brightness + 127) / 255,
        (uint16_t(p.g) * p.brightness + 127) / 255,
        (uint16_t(p.b) * p.brightness + 127) / 255);
    if (strip_.getPixelColor(i) != color) {
      strip_.setPixelColor(i, color);
      changed = true;
    }
  }
  if (changed) {
    strip_.show();
    lastAppliedMs = millis();
  }
}
