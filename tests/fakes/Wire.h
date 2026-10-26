#pragma once
#include <stdint.h>
struct FakeWire {
  bool failure = false;
  uint8_t data[16]{};
  unsigned address = 0, cursor = 0, transactions = 0;
  unsigned timeout = 0, sda = 0, scl = 0;
  uint8_t writes[16]{};
  unsigned writeCount = 0;
  void setSDA(unsigned pin) { sda = pin; }
  void setSCL(unsigned pin) { scl = pin; }
  void begin() {}
  void setClock(unsigned) {}
  void setTimeout(unsigned ms, bool) { timeout = ms; }
  void beginTransmission(unsigned) { ++transactions; writeCount=0; }
  void write(uint8_t byte) { writes[writeCount++]=byte; if(writeCount==1) address=byte; }
  int endTransmission() {
    if (failure) return 4;
    for (unsigned i=1;i<writeCount;++i) data[address+i-1]=writes[i];
    return 0;
  }
  unsigned requestFrom(uint8_t, uint8_t count) { cursor = address; return failure ? 0 : count; }
  int read() { return data[cursor++]; }
};
inline FakeWire Wire;
