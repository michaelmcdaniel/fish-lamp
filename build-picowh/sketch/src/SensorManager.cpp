#line 1 "C:\\Users\\mmcda\\OneDrive\\Code\\pico\\fish-lamp\\src\\SensorManager.cpp"
#include "SensorManager.h"
#include "MockDebug.h"
#include <Arduino.h>
#include <cmath>
namespace {
void sample(SensorReading& reading,float value,bool good,uint32_t now) {
  reading.healthy=good;
  if(good) {reading.value=value;reading.valid=true;reading.lastSuccessMs=now;}
}
void mock(SensorReading& reading,float value,uint32_t now) {
  reading.mocked=true; sample(reading,value,true,now);
}
}
void SensorManager::begin() {
#if FISH_LAMP_WATER
  waterBus_.begin(devices::waterPin);
#endif
  if(devices::ambient) analogReadResolution(12);
  if(!devices::water) mockdebug::log("mock water=%.1f F\r\n",double(devices::mockWaterF));
  if(!devices::ambient) mockdebug::log("mock ambient=%.1f percent\r\n",double(devices::mockAmbientPercent));
  if(!devices::cpu) mockdebug::log("mock CPU=%.1f C\r\n",double(devices::mockCpuC));
}
void SensorManager::tick(uint32_t now) {
#if FISH_LAMP_WATER
  if(converting_ && uint32_t(now-conversionMs_)>=750) {
    converting_=false;
    uint8_t data[9]{};
    bool good=waterBus_.read_bit()!=0 && waterBus_.reset()!=0;
    if(good) {
      waterBus_.select(address_); waterBus_.write(0xbe);
      for(auto& byte:data) byte=waterBus_.read();
      good=OneWire::crc8(data,8)==data[8] && data[5]==0xff && data[7]==0x10;
    }
    int16_t raw=int16_t(uint16_t(data[0]) | uint16_t(data[1])<<8);
    // Undefined low bits at reduced resolution must be masked.
    unsigned resolution=(data[4]>>5)&3;
    raw=int16_t(raw & ~((1<<(3-resolution))-1));
    float c=float(raw)/16.0f;
    sample(waterF,c*1.8f+32.0f,good && c>=-55 && c<=125,now);
  }
#endif
  if(!first_ && uint32_t(now-attemptedMs_)<60000) return;
  first_=false; attemptedMs_=now;
  if(!devices::water) mock(waterF,devices::mockWaterF,now);
#if FISH_LAMP_WATER
  // Exactly one externally powered DS18B20 on GPIO 26. READ ROM avoids a
  // potentially long bus search. Missing/bad devices are retried next minute.
  bool good=waterBus_.reset()!=0;
  if(good) {
    waterBus_.write(0x33);
    for(auto& byte:address_) byte=waterBus_.read();
    good=address_[0]==0x28 && OneWire::crc8(address_,7)==address_[7];
  }
  if(good && waterBus_.reset()) {
    waterBus_.select(address_); waterBus_.write(0xb4);
    good=waterBus_.read_bit()!=0; // Parasite power is deliberately unsupported.
  } else good=false;
  if(good && waterBus_.reset()) {
    waterBus_.select(address_); waterBus_.write(0x44,0);
    conversionMs_=millis(); converting_=true;
  } else waterF.healthy=false;
#endif
  if(devices::ambient) {
    int raw=analogRead(devices::ambientPin);
    sample(ambientPercent,float(raw)*100.0f/4095.0f,raw>=0 && raw<=4095,now);
  } else mock(ambientPercent,devices::mockAmbientPercent,now);
  if(devices::cpu) {
    float sum=0;
    for(unsigned i=0;i<8;++i) sum+=analogReadTemp();
    float c=sum/8;
    sample(cpuC,c,std::isfinite(c) && c>=-40 && c<=125,now);
  } else mock(cpuC,devices::mockCpuC,now);
}
