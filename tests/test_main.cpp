#include "ClockMath.h"
#include "Config.h"
#include "ScheduleController.h"
#include "TimeService.h"
#include "LightingController.h"
#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <fstream>
#include <iterator>

using namespace clockmath;
unsigned testSetup(const std::string& fixture);
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; return 1; } } while (0)
static std::string fixture() {
  JsonDocument d;
  d["schema-version"] = 1; d["rtc-utc-confirmed"] = true;
  d["latitude"] = 0; d["longitude"] = 0;
  d["timezone-offset-minutes"] = 0; d["timezone-dst"] = "none";
  for (const char* name : {"lights", "nightlights"}) {
    auto a = d[name].to<JsonArray>();
    for (unsigned i = 0; i < 85; ++i) {
      auto row = a.add<JsonArray>();
      for (int value : {16, 32, 64, 128}) row.add(value);
    }
  }
  for (const char* name : {"lights-sunrise-offset", "lights-sunset-offset",
                           "growlight-sunrise-offset", "growlight-sunset-offset"}) d[name] = 0;
  d["lights-max-offset"] = 86400; d["growlight-max-offset"] = 86400;
  std::string s; serializeJson(d, s); return s;
}
int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--validate") {
    std::ifstream file(argv[2], std::ios::binary);
    if (!file) return 2;
    file.seekg(0,std::ios::end);
    if (file.tellg() > std::streamoff(MaxConfigBytes)) return 2;
    file.seekg(0);
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Config candidate;
    bool ok = parseConfig(data.data(),data.size(),candidate);
    std::cout << (ok ? "Configuration valid\n" : "Configuration invalid\n");
    return ok ? 0 : 1;
  }
  CHECK(valid({2024,2,29})); CHECK(!valid({2023,2,29})); CHECK(!valid({2100,2,29}));
  for (int y = 2000; y < 2100; ++y) for (int m = 1; m <= 12; ++m)
    for (int d = 1; d <= 31; ++d) if (valid({y,m,d})) {
      Date r = dateAt(midnight({y,m,d}));
      CHECK(r.year == y && r.month == m && r.day == d);
    }
  CHECK(dateAt(midnight({2024,3,1}) - 1).day == 29);
  Zone us{-360, true};
  int64_t spring = midnight({2026,3,8}) + 8*3600;
  int64_t fall = midnight({2026,11,1}) + 7*3600;
  CHECK(offsetAt(spring-1, us) == -21600); CHECK(offsetAt(spring, us) == -18000);
  CHECK(offsetAt(fall-1, us) == -18000); CHECK(offsetAt(fall, us) == -21600);
  CHECK(localToUtc(midnight({2026,11,1}) + 5400, us) == fall - 1800);
  CHECK(localToUtc(midnight({2026,3,8}) + 9000, us) == spring + 1800);
  CHECK(offsetAt(spring, {330,false}) == 19800);
  uint8_t rtc[] = {0x59,0x59,0x23,4,0x29,0x02,0x24};
  int64_t epoch = 0;
  CHECK(decodeRtc(rtc,0,epoch)); CHECK(epoch == midnight({2024,3,1})-1);
  CHECK(!decodeRtc(rtc,0x80,epoch));
  rtc[6]=0x23; CHECK(!decodeRtc(rtc,0,epoch)); rtc[6]=0x24;
  rtc[0]=0x6a; CHECK(!decodeRtc(rtc,0,epoch)); rtc[0]=0;
  rtc[2]=0x72; CHECK(decodeRtc(rtc,0,epoch)); CHECK(epoch%86400 == 12*3600+59*60);
  rtc[2]=0x52; CHECK(decodeRtc(rtc,0,epoch)); CHECK(epoch%86400 == 59*60);
  rtc[5]=0x82; CHECK(!decodeRtc(rtc,0,epoch));
  Window w{100,200};
  CHECK(!w.contains(99)); CHECK(w.contains(100)); CHECK(w.contains(199)); CHECK(!w.contains(200));
  CHECK(!Window({200,100}).contains(150));
  auto s = solarSchedule({2026,3,20},0,0,{}, {}, {});
  auto day = midnight({2026,3,20});
  CHECK(s.solarValid);
  CHECK(std::abs(double(s.lights.on-day-6*3600)) < 15*60);
  CHECK(std::abs(double(s.lights.off-day-18*3600)) < 15*60);
  auto cap = solarSchedule({2026,3,20},0,0,{}, {3600,3600,17*3600}, {});
  CHECK(cap.lights.on == s.lights.on+3600); CHECK(cap.lights.off == day+17*3600);
  CHECK(cap.grow.on == s.grow.on && cap.grow.off == s.grow.off);
  CHECK(!solarSchedule({2026,6,21},90,0,{}, {}, {}).solarValid);
  CHECK(!solarSchedule({2026,12,21},80,0,{}, {}, {}).solarValid);
  CHECK(!solarSchedule({2026,6,21},NAN,0,{}, {}, {}).solarValid);
  TimeService time;
  fakeMillis = 0xffff0000u;
  time.begin();
  CHECK(Wire.sda == 16 && Wire.scl == 17 && Wire.timeout == 5);
  Wire.failure = true;
  time.tick(fakeMillis); CHECK(!time.valid());
  Wire.failure = false;
  const uint8_t healthy[] = {0,0,0,1,0x20,0x03,0x26};
  for (unsigned i = 0; i < 7; ++i) Wire.data[i] = healthy[i];
  fakeMillis += 60000; time.tick(fakeMillis); CHECK(time.valid() && time.rtcHealthy());
  int64_t original = time.utc();
  Wire.failure = true;
  for (unsigned second = 1; second <= 48*3600; ++second) {
    fakeMillis += 1000; time.tick(fakeMillis);
    CHECK(time.valid() && time.utc() == original+second);
  }
  CHECK(!time.rtcHealthy());
  CHECK(Wire.transactions < 3000); // Retries remain minute-spaced during outage.
  Wire.failure = false; fakeMillis += 60000; time.tick(fakeMillis);
  CHECK(time.rtcHealthy() && time.utc() == original);
  Wire.data[15] = 0x80; fakeMillis += 60000; time.tick(fakeMillis);
  CHECK(!time.rtcHealthy() && time.utc() == original+60);
  ManualOverride manual;
  manual.request(false,true,0xfffffff0u,200);
  manual.tick(30,199); CHECK(manual.active && manual.forceOff);
  manual.tick(31,200); CHECK(!manual.active);
  manual.request(true,false,0xfffffff0u,0);
  manual.tick(uint32_t(0xfffffff0u + 3599999u),0); CHECK(manual.active);
  manual.tick(uint32_t(0xfffffff0u + 3600000u),0); CHECK(!manual.active);
  Config c;
  LightingController hardware;
  hardware.begin();
  CHECK(hardware.ready() && fakePin == 8 && fakeModePin == 8 && fakeLevel == LOW);
  CHECK(fakeStripPin == 7 && fakeStripCount == 85 && fakeStripOrder == NEO_GRB+NEO_KHZ800);
  CHECK(fakeShows == 1);
  c.lights[84] = {255,128,0,128};
  hardware.apply(c.lights,true);
  CHECK(fakePixels[84] == 0x804000 && fakeLevel == HIGH);
  CHECK(fakeShows == 2 && fakeWrites == 2);
  for (int i = 0; i < 1000; ++i) hardware.apply(c.lights,true);
  CHECK(fakeShows == 2 && fakeWrites == 2);
  hardware.apply(nullptr,false);
  CHECK(fakeShows == 3 && fakeWrites == 3 && fakePixels[84] == 0 && fakeLevel == LOW);
  std::string good = fixture();
  CHECK(parseConfig(good.data(),good.size(),c)); CHECK(c.lights[84].b == 64);
  CHECK(!parseConfig("{}",2,c)); CHECK(c.lights[84].b == 64);
  for (size_t n = 0; n < good.size(); ++n) CHECK(!parseConfig(good.data(),n,c));
  for (const char* field : {"latitude", "timezone-dst", "rtc-utc-confirmed", "lights",
                            "lights-max-offset", "growlight-sunrise-offset"}) {
    JsonDocument d; deserializeJson(d,good); d.remove(field);
    std::string bad; serializeJson(d,bad); CHECK(!parseConfig(bad.data(),bad.size(),c));
  }
  for (int value : {-1,256}) {
    JsonDocument d; deserializeJson(d,good); d["lights"][84][3]=value;
    std::string bad; serializeJson(d,bad); CHECK(!parseConfig(bad.data(),bad.size(),c));
  }
  for (int variation = 0; variation < 9; ++variation) {
    JsonDocument d; deserializeJson(d,good);
    switch (variation) {
      case 0: d["lights"].as<JsonArray>().remove(84); break;
      case 1: d["nightlights"].as<JsonArray>().add<JsonArray>(); break;
      case 2: d["lights"][0][0] = 1.5; break;
      case 3: d["latitude"] = 91; break;
      case 4: d["timezone-dst"] = "unknown"; break;
      case 5: d["lights-sunset-offset"] = 43201; break;
      case 6: d["rtc-utc-confirmed"] = false; break;
      case 7: d["growlight-max-offset"] = "86400"; break;
      case 8: d["schema-version"] = 2; break;
    }
    std::string bad; serializeJson(d,bad); CHECK(!parseConfig(bad.data(),bad.size(),c));
  }
  for (const std::string suffix : {"garbage", "{}", " true"}) {
    std::string bad = good + suffix; CHECK(!parseConfig(bad.data(),bad.size(),c));
  }
  std::string huge(MaxConfigBytes+1,' '); CHECK(!parseConfig(huge.data(),huge.size(),c));
  ScheduleController control;
  control.tick(s.lights.on,0,c); CHECK(control.dayOn && control.growOn);
  control.lightsOverride.request(false,true,0,control.nextLightsBoundary);
  control.tick(s.lights.on+10,10,c); CHECK(!control.dayOn && control.forceOff && control.growOn);
  control.tick(s.lights.on+3600,3600000,c); CHECK(control.dayOn && !control.forceOff);
  control.tick(s.lights.off,3600001,c); CHECK(!control.dayOn && !control.growOn);
  // 48 offline hours at minute resolution: no networking linked into this test.
  for (int minute = 0; minute < 48*60; ++minute) {
    int64_t now = day + minute*60;
    auto expected = solarSchedule(dateAt(now),0,0,{}, {}, {});
    control.tick(now,uint32_t(minute)*60000,c);
    CHECK(control.dayOn == expected.lights.contains(now));
    CHECK(control.growOn == expected.grow.contains(now));
  }
  // Tomorrow's negative sunrise offset must operate before today's midnight.
  c.lightsOffsets.sunrise = -43200;
  ScheduleController overnight;
  auto tomorrow = solarSchedule({2026,3,21},0,0,{}, c.lightsOffsets, c.growOffsets);
  overnight.tick(day+23*3600,0,c);
  CHECK(tomorrow.lights.contains(day+23*3600) && overnight.dayOn);
  checks += testSetup(good);
  std::cout << checks << " checks passed\n";
}
