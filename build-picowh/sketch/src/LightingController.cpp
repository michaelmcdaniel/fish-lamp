#line 1 "C:\\Users\\mmcda\\OneDrive\\Code\\pico\\fish-lamp\\src\\LightingController.cpp"
#include "LightingController.h"
#include "MockDebug.h"

void LightingController::begin() {
  if(devices::grow) { digitalWrite(devices::growPin, LOW); pinMode(devices::growPin, OUTPUT); }
  else mockdebug::log("mock grow=0\r\n");
#if FISH_LAMP_STRIP
  ready_ = strip_.begin();
  if (ready_) {
    strip_.clear();
    strip_.show();
    lastAppliedMs = millis();
  }
#else
  ready_=true;
  mockdebug::log("mock strip: 85 pixels black\r\n");
#endif
}

void LightingController::apply(const Pixel* pixels, bool growOn) {
  if (growOn_ != growOn) {
    if(devices::grow) digitalWrite(devices::growPin, growOn ? HIGH : LOW);
    else mockdebug::log("mock grow=%d\r\n",growOn);
    growOn_ = growOn;
    lastAppliedMs = millis();
  }
  if (!ready_) return;
  bool changed = false;
  for (unsigned i = 0; i < PixelCount; ++i) {
    const Pixel p = pixels ? pixels[i] : Pixel{0, 0, 0, 0};
    const uint32_t color = Adafruit_NeoPixel::Color(
        (uint16_t(p.r) * p.brightness + 127) / 255,
        (uint16_t(p.g) * p.brightness + 127) / 255,
        (uint16_t(p.b) * p.brightness + 127) / 255);
    if (applied_[i] != color) {
      applied_[i]=color;
#if FISH_LAMP_STRIP
      strip_.setPixelColor(i, color);
#else
      mockdebug::log("mock pixel[%u]=%06lx\r\n",i,static_cast<unsigned long>(color));
#endif
      changed = true;
    }
  }
  if (changed) {
#if FISH_LAMP_STRIP
    strip_.show();
#endif
    lastAppliedMs = millis();
  }
}
