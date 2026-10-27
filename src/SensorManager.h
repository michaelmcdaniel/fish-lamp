#pragma once
#include "Devices.h"
#include <stdint.h>
#if FISH_LAMP_WATER
#include <OneWire.h>
#endif
struct SensorReading {
  float value=0;
  bool valid=false, healthy=false, mocked=false;
  uint32_t lastSuccessMs=0;
};
class SensorManager {
 public:
  void begin();
  void tick(uint32_t now);
  SensorReading waterF, ambientPercent, cpuC;
 private:
#if FISH_LAMP_WATER
  OneWire waterBus_;
  uint8_t address_[8]{};
#endif
  bool first_=true, ambientFirst_=true, converting_=false;
  uint32_t attemptedMs_=0, conversionMs_=0, ambientMs_=0;
};
