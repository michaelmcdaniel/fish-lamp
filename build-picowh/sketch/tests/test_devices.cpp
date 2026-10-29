#line 1 "C:\\Users\\mmcda\\OneDrive\\Code\\pico\\fish-lamp\\tests\\test_devices.cpp"
#include "Devices.h"
#include "SensorManager.h"
#include "LightingController.h"
#include "TimeService.h"
#include "StatusLed.h"
#include "MockDebug.h"
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
int main() {
  SensorManager sensors; sensors.begin();
  auto tick=[&](uint32_t now) {fakeMillis=now;sensors.tick(now);};
  tick(0);
  if(devices::water) {
    assert(!sensors.waterF.valid && fakeConversions==1);
    tick(749);assert(!sensors.waterF.valid);
    tick(750);assert(sensors.waterF.valid && sensors.waterF.healthy);
    assert(std::abs(sensors.waterF.value-77)<0.01);
    fakeBusPresent=false;tick(60000);
    assert(!sensors.waterF.healthy && sensors.waterF.valid && sensors.waterF.value==77);
    fakeBusPresent=true;fakeBadCrc=true;tick(120000);assert(!sensors.waterF.healthy);
    fakeBadCrc=false;fakeParasite=true;tick(180000);assert(!sensors.waterF.healthy);
    fakeParasite=false;tick(240000);tick(240750);assert(sensors.waterF.healthy);
    tick(300000);fakeBadCrc=true;tick(300750);assert(!sensors.waterF.healthy);
    fakeBadCrc=false;tick(360000);fakeConversionReady=false;
    tick(360750);assert(!sensors.waterF.healthy);
    fakeConversionReady=true;
    SensorManager wrap;wrap.begin();fakeMillis=0xfffffff0u;wrap.tick(fakeMillis);
    fakeMillis+=749;wrap.tick(fakeMillis);assert(!wrap.waterF.valid);
    ++fakeMillis;wrap.tick(fakeMillis);assert(wrap.waterF.valid);
  } else {
    assert(fakeBusBegins==0 && fakeBusResets==0);
    assert(sensors.waterF.mocked && sensors.waterF.value==devices::mockWaterF);
  }
  assert(sensors.ambientPercent.valid && sensors.cpuC.valid);
  if(!devices::ambient) assert(fakeAdcReads==0 && sensors.ambientPercent.mocked);
  if(!devices::cpu) assert(fakeCpuReads==0 && sensors.cpuC.mocked);
  if(devices::ambient) {fakeAdc=-1;tick(420000);assert(!sensors.ambientPercent.healthy && sensors.ambientPercent.valid);}
  if(devices::cpu) {fakeCpu=NAN;tick(480000);assert(!sensors.cpuC.healthy && sensors.cpuC.valid);}
  fakeWrites=0;fakeShows=0;
  LightingController lights;lights.begin();
  Pixel pixels[PixelCount]{};pixels[0]={255,0,0,128};
  lights.apply(pixels,true);
  assert(lights.ready());
  auto writes=fakeWrites, shows=fakeShows;
  lights.apply(pixels,true);assert(fakeWrites==writes && fakeShows==shows);
  if(!devices::grow) assert(fakeWrites==0);
  if(!devices::strip) assert(fakeShows==0);
  else assert(fakePixels[0]==0x800000);
  if(!devices::rtc) {
    fakeMillis=0;TimeService time;time.begin();assert(time.valid() && !time.rtcHealthy());
    time.tick(1500);assert(time.utc()==devices::mockRtcUtc+1);
    assert(time.setUtc(devices::mockRtcUtc+100,2000));time.tick(3000);
    assert(time.utc()==devices::mockRtcUtc+101 && Wire.transactions==0);
    assert(!time.setUtc(0,4000));
  }
  fakeWrites=0;
  StatusLed led;led.begin();led.apply(true);auto ledWrites=fakeWrites;
  led.apply(true);assert(fakeWrites==ledWrites);
  if(!devices::status) assert(fakeWrites==0);
  char bytes[4096];size_t n=mockdebug::take(bytes,sizeof(bytes));std::string log(bytes,n);
  if(!devices::strip) assert(log.find("mock pixel[0]=800000")!=std::string::npos);
  if(!devices::grow) assert(log.find("mock grow=1")!=std::string::npos);
  for(unsigned i=0;i<1000;++i) mockdebug::log("mock bounded queue test\r\n");
  assert(mockdebug::dropped()>0 && mockdebug::take(bytes,sizeof(bytes))<=4096);
  std::cout << "Device adapter tests passed (real flags " << devices::strip << devices::grow << devices::rtc << devices::water << devices::ambient << devices::cpu << devices::status << ")\n";
}
