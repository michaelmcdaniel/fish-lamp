#pragma once
#include "Arduino.h"
#include <array>
constexpr int NEO_GRB = 1, NEO_KHZ800 = 2;
inline unsigned fakeShows = 0;
inline int fakeStripPin = -1, fakeStripOrder = -1, fakeStripCount = 0;
inline std::array<uint32_t,85> fakePixels{};
class Adafruit_NeoPixel {
 public:
  Adafruit_NeoPixel(int count, int pin, int order) {
    fakeStripCount=count; fakeStripPin=pin; fakeStripOrder=order;
  }
  bool begin() { return true; }
  void clear() { fakePixels.fill(0); }
  void show() { ++fakeShows; }
  static uint32_t Color(uint8_t r,uint8_t g,uint8_t b) { return (uint32_t(r)<<16) | (uint32_t(g)<<8) | b; }
  uint32_t getPixelColor(unsigned i) const { return fakePixels.at(i); }
  void setPixelColor(unsigned i,uint32_t color) { fakePixels.at(i)=color; }
};
