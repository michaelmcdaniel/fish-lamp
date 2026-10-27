#include "StatusDisplay.h"
#include "ConfigManager.h"
#include "TimeService.h"
#include "LightingController.h"
#include "ScheduleController.h"
#include "SensorManager.h"
#include "SetupService.h"
#include <Wire.h>
#include <cassert>
#include <iostream>
#include <string>

int main() {
  ConfigManager settings;
  TimeService clock;
  LightingController lights;
  ScheduleController schedule;
  SensorManager sensors;
  SetupService network;
  lights.begin();
  // Establish real RTC-like state without needing an RTC for display tests.
  clock.begin();
  assert(clock.setUtc(1767268800,0));
  settings.state=ConfigState::Primary;
  settings.config.zone={-360,false};
  schedule.dayOn=true; schedule.solarValid=true;
  sensors.waterF={77,true,true,false,0};
  sensors.ambientPercent={50,true,true,false,0};
  sensors.cpuC={30,true,true,false,0};
  Pixel pixels[PixelCount]{};
  pixels[0]={255,0,0,128}; lights.apply(pixels,true);
  assert(lights.growOn() && lights.minBrightness()==0 && lights.maxBrightness()==128);
  const auto rtcTransactions=Wire.transactions, rtcBegins=Wire.begins;
  StatusDisplay display;
  display.begin(0);
  auto tick=[&](uint32_t now) {
    auto before=Wire1.transactions;
    display.tick(now,settings,clock,schedule,lights,network,sensors);
    assert(Wire1.transactions-before<=1); // No full-frame blocking transfer.
    assert(Wire.transactions==rtcTransactions && Wire.begins==rtcBegins);
    assert(Wire1.writeCount<=24); // <=23 command bytes or 17 data bytes.
  };
#if FISH_LAMP_OLED
  assert(Wire1.begins==1 && Wire1.sda==2 && Wire1.scl==3);
  assert(Wire1.timeout==5 && Wire1.clock==100000);
  tick(99);assert(Wire1.transactions==0);
  tick(100);assert(Wire1.target==devices::oledAddress);
  assert(fakeOledLines[0]=="Fish Lamp");
  // Eight pages: one positioning + eight 16-byte writes per page, then on.
  for(unsigned i=0;i<73;++i) tick(100);
  assert(Wire1.transactions==74);
  tick(1099);assert(fakeOledLines[0]=="Fish Lamp");
  tick(1100);
  assert(fakeOledLines[0]=="WiFi: disabled");
  assert(fakeOledLines[1]=="RTC OK 06:00:00 L");
  assert(fakeOledLines[2]=="Water: 77.0F");
  assert(fakeOledLines[3]=="Light: 50.0%");
  assert(fakeOledLines[4]=="Grow: ON");
  assert(fakeOledLines[5]=="RGB DAY B:0-128");
  assert(fakeOledLines[6]=="Up 0d 00:00:01");
  for(unsigned i=0;i<72;++i) tick(1100);
  auto before=Wire1.transactions;
  tick(2099);assert(Wire1.transactions==before);
  tick(2100); // Only uptime text changed; no synchronous write.
  assert(Wire1.transactions==before);
  for(unsigned i=0;i<9;++i) tick(2100);
  assert(Wire1.transactions==before+9); // Exactly one changed row sent.
  tick(2100);assert(Wire1.transactions==before+9);

  sensors.waterF.healthy=false;
  sensors.ambientPercent.valid=false; sensors.ambientPercent.healthy=false;
  schedule.forceOff=true; lights.apply(nullptr,false);
  tick(3100);
  assert(fakeOledLines[2]=="Water: 77.0F STALE");
  assert(fakeOledLines[3]=="Light: -- unavailable");
  assert(fakeOledLines[4]=="Grow: OFF");
  assert(fakeOledLines[5]=="RGB OFF B:0-0");
  assert(fakeOledLines[7].find("Ambient unavailable")!=std::string::npos);
  for(unsigned i=0;i<72;++i) tick(3100);
  sensors.waterF.healthy=true;
  sensors.ambientPercent.valid=true; sensors.ambientPercent.healthy=true;
  tick(4100);
  assert(fakeOledLines[7].find("Ambient unavailable")!=std::string::npos);
  for(unsigned i=0;i<72;++i) tick(4100);
  // A failed data transaction disables all subsequent display I/O.
  tick(5100); tick(5100);
  Wire1.failure=true; tick(5100);
  assert(display.failed()); before=Wire1.transactions;
  for(unsigned i=0;i<100;++i) tick(100000+i*1000);
  assert(Wire1.transactions==before);

  Wire1=FakeWire{}; Wire1.failure=true;
  StatusDisplay missing;missing.begin(0);
  missing.tick(100,settings,clock,schedule,lights,network,sensors);
  assert(missing.failed() && Wire1.transactions==1);
  missing.tick(60100,settings,clock,schedule,lights,network,sensors);
  assert(Wire1.transactions==1);

  Wire1=FakeWire{};
  StatusDisplay wrap;wrap.begin(0xfffffff0u);
  auto advance=[&](uint32_t now) { wrap.tick(now,settings,clock,schedule,lights,network,sensors); };
  advance(0x54);for(unsigned i=0;i<73;++i) advance(0x54);
  advance(0x43c);
  assert(fakeOledLines[6]=="Up 49d 17:02:48");
#else
  for(unsigned i=0;i<100;++i) tick(i*1000);
  assert(Wire1.begins==0 && Wire1.transactions==0 && !display.failed());
#endif
  std::cout << "Status display tests passed (OLED=" << FISH_LAMP_OLED << ")\n";
}
