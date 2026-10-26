#pragma once
#include <stdint.h>
inline unsigned fakeBusBegins=0, fakeBusResets=0, fakeConversions=0;
inline bool fakeBusPresent=true, fakeParasite=false, fakeBadCrc=false;
inline bool fakeConversionReady=true;
class OneWire {
 public:
  void begin(int) {++fakeBusBegins;}
  int reset() {++fakeBusResets;return fakeBusPresent;}
  void select(const uint8_t*) {}
  void write(uint8_t command,int=0) {command_=command;cursor_=0;if(command==0x44)++fakeConversions;}
  int read_bit() {return command_==0x44 ? fakeConversionReady : !fakeParasite;}
  static uint8_t crc8(const uint8_t* bytes,uint8_t count) {
    uint8_t crc=0;
    while(count--) {uint8_t byte=*bytes++;for(unsigned j=0;j<8;++j) {bool mix=(crc^byte)&1;crc>>=1;if(mix)crc^=0x8c;byte>>=1;}}
    return crc;
  }
  uint8_t read() {
    uint8_t rom[8]={0x28,1,2,3,4,5,6,0}; rom[7]=crc8(rom,7);
    uint8_t data[9]={0x90,0x01,0,0,0x7f,0xff,0x0c,0x10,0};data[8]=crc8(data,8);
    if(fakeBadCrc) {rom[7]^=1;data[8]^=1;}
    return command_==0x33 ? rom[cursor_++%8] : data[cursor_++%9];
  }
 private:
  uint8_t command_=0;unsigned cursor_=0;
};
