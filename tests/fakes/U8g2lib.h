#pragma once
#include <array>
#include <cassert>
#include <cstring>
#include <string>
#include <stdint.h>
constexpr int U8G2_R0=0, U8X8_PIN_NONE=255;
inline uint8_t u8g2_font_5x8_tf[1]{};
inline std::array<std::string,8> fakeOledLines;
class U8G2_SH1106_128X64_NONAME_F_HW_I2C {
 public:
  U8G2_SH1106_128X64_NONAME_F_HW_I2C(int,int) {}
  void setFont(const uint8_t*) {}
  void setFontPosBaseline() {}
  void clearBuffer() { buffer_.fill(0); fakeOledLines.fill(""); }
  void drawStr(unsigned x,unsigned y,const char* line) {
    assert(x==0 && y%8==6 && y<64 && strlen(line)<=25);
    fakeOledLines[y/8]=line;
    // Deterministic stand-in for rasterization; tests cover transport and text,
    // not the actual U8g2 font glyphs or physical panel orientation.
    memcpy(buffer_.data()+(y/8)*128,line,strlen(line));
  }
  uint8_t* getBufferPtr() { return buffer_.data(); }
 private:
  std::array<uint8_t,1024> buffer_{};
};
