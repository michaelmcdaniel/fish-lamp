#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cassert>
struct FakeWire {
  bool failure = false;
  uint8_t data[16]{};
  unsigned address = 0, cursor = 0, transactions = 0;
  unsigned timeout = 0, sda = 0, scl = 0, begins = 0, clock = 0, target = 0;
  uint8_t writes[32]{};
  unsigned writeCount = 0;
  void setSDA(unsigned pin) { sda = pin; }
  void setSCL(unsigned pin) { scl = pin; }
  void begin() { ++begins; }
  void setClock(unsigned hz) { clock=hz; }
  void setTimeout(unsigned ms, bool) { timeout = ms; }
  void beginTransmission(unsigned device) { ++transactions; writeCount=0; target=device; }
  size_t write(uint8_t byte) {
    assert(writeCount < sizeof(writes));
    writes[writeCount++]=byte; if(writeCount==1) address=byte;
    return 1;
  }
  size_t write(const uint8_t* bytes, size_t count) {
    for(size_t i=0;i<count;++i) write(bytes[i]);
    return count;
  }
  int endTransmission() {
    if (failure) return 4;
    if(target != 0x68) return 0; // OLED transfers do not mutate RTC registers.
    for (unsigned i=1;i<writeCount;++i) data[address+i-1]=writes[i];
    return 0;
  }
  unsigned requestFrom(uint8_t, uint8_t count) { cursor = address; return failure ? 0 : count; }
  int read() { return data[cursor++]; }
};
inline FakeWire Wire;
inline FakeWire Wire1;
