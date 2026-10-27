#include "WebService.h"
#include <ArduinoJson.h>
#include <Arduino.h>
#include <Wire.h>
#include <iostream>
#include <cstdlib>
#include <limits>

static unsigned assertions=0;
#define REQUIRE(x) do{++assertions;if(!(x)){std::cerr<<"FAIL web line "<<__LINE__<<": " #x "\n";std::exit(1);}}while(0)
unsigned testWeb(const std::string& fixture) {
  WiFi=FakeWiFi{};Wire=FakeWire{};fakeIncoming.clear();fakeMillis=100;
  ConfigManager settings;REQUIRE(parseConfig(fixture.data(),fixture.size(),settings.config));
  settings.state=ConfigState::Primary;
  snprintf(settings.config.wifiSsid,sizeof(settings.config.wifiSsid),"%s","test-\"network");
  snprintf(settings.config.wifiPassword,sizeof(settings.config.wifiPassword),"%s","synthetic-private-marker");
  TimeService clock;clock.begin();REQUIRE(clock.setUtc(clockmath::midnight({2026,3,20})+43200,fakeMillis));
  ScheduleController schedule;LightingController lighting;lighting.begin();SensorManager sensors;
  sensors.waterF={77,true,false,false,50}; // Last good reading retained after failure.
  sensors.ambientPercent={50,true,true,true,50};
  sensors.cpuC.value=std::numeric_limits<float>::quiet_NaN();
  SetupService network;network.begin(settings);
  WebService web;
  auto tick=[&]() {
    ++fakeMillis;network.tick(fakeMillis,settings,clock);
    schedule.tick(clock.utc(),fakeMillis,settings.config);
    lighting.apply(schedule.dayOn?settings.config.lights:settings.config.nightlights,schedule.growOn);
    web.tick(fakeMillis,settings,clock,schedule,lighting,network,sensors);
  };
  auto socket=[](const std::string& input) {
    auto s=std::make_shared<FakeSocket>();s->input=input;fakeIncoming.push_back(s);return s;
  };
  auto get=[](const char* path) {return std::string("GET ")+path+" HTTP/1.1\r\nHost: 192.0.2.42\r\n\r\n";};
  auto exchange=[&](const std::string& request) {
    auto s=socket(request);for(unsigned i=0;i<12000 && s->live;++i)tick();
    REQUIRE(!s->live && s->stopWait==1);return s->output;
  };
  tick();REQUIRE(network.connecting());
  WiFi.statusCode=WL_CONNECTED;tick();REQUIRE(network.connected());
  auto page=exchange(get("/"));REQUIRE(page.find("200 OK")!=std::string::npos);
  REQUIRE(page.find("fetch('/status'")!=std::string::npos && page.find("https://")==std::string::npos);
  auto response=exchange(get("/status"));
  REQUIRE(response.find("200 OK")!=std::string::npos);
  auto split=response.find("\r\n\r\n");REQUIRE(split!=std::string::npos);
  JsonDocument d;REQUIRE(!deserializeJson(d,response.substr(split+4)));
  REQUIRE(d["lights"].size()==85 && d["nightlights"].size()==85);
  REQUIRE(d["lights"][84][3]==128 && d["waterTemp"]==77);
  REQUIRE(d["sensors"]["waterF"]["healthy"]==false && d["sensors"]["cpuC"]["value"].isNull());
  REQUIRE(d["wifi"]["ssid"]=="test-\"network" && d["wifi"]["ip"]=="192.0.2.42");
  REQUIRE(response.find("synthetic-private-marker")==std::string::npos);
  REQUIRE(d["schedule"]["lightsOn"].as<int64_t>()>clock.utc());
  REQUIRE(d["schedule"]["lightsOff"].as<int64_t>()>clock.utc());
  int64_t on=0,off=0;schedule.nextTransitions(clock.utc(),false,on,off);
  REQUIRE(on==d["schedule"]["lightsOn"] && off==d["schedule"]["lightsOff"]);
  // Tomorrow's advanced sunrise overlaps today's window: no false off edge.
  Config overlapConfig=settings.config;overlapConfig.lightsOffsets.sunrise=-43200;
  overlapConfig.lightsOffsets.sunset=43200;ScheduleController overlap;
  overlap.tick(clock.utc(),fakeMillis,overlapConfig);
  overlap.nextTransitions(clock.utc(),false,on,off);
  const auto today=solarSchedule({2026,3,20},0,0,{},overlapConfig.lightsOffsets,{});
  REQUIRE(off!=today.lights.off);
  REQUIRE(exchange(get("/api/status")).find("200 OK")!=std::string::npos);
  REQUIRE(exchange(get("/session")).find("404 Error")!=std::string::npos);
  REQUIRE(exchange("POST /configure HTTP/1.1\r\nHost: 192.0.2.42\r\n\r\n").find("405 Error")!=std::string::npos);
  REQUIRE(exchange("GET /status HTTP/1.1\r\nHost: lamp\r\nContent-Length: 1\r\n\r\nx").find("400 Error")!=std::string::npos);
  REQUIRE(exchange("GET /status HTTP/1.1\r\nHost: lamp\r\nX: "+std::string(2100,'x')+"\r\n\r\n").find("431 Error")!=std::string::npos);
  auto slow=socket("GET /");for(unsigned i=0;i<2100;++i)tick();REQUIRE(!slow->live);
  auto blocked=socket(get("/"));blocked->room=0;
  for(unsigned i=0;i<2100;++i)tick();REQUIRE(!blocked->live && schedule.dayOn);
  auto disconnected=socket(get("/status"));tick();WiFi.statusCode=0;tick();
  REQUIRE(!disconnected->live && !network.connected() && schedule.dayOn);
  fakeMillis+=60000;tick();WiFi.statusCode=WL_CONNECTED;tick();
  REQUIRE(exchange(get("/status")).find("200 OK")!=std::string::npos);
  Wire.failure=true;TimeService invalid;invalid.begin();
  WebService invalidWeb;
  auto missing=socket(get("/status"));
  for(unsigned i=0;i<1000 && missing->live;++i)invalidWeb.tick(++fakeMillis,settings,invalid,schedule,lighting,network,sensors);
  split=missing->output.find("\r\n\r\n");REQUIRE(split!=std::string::npos);
  REQUIRE(!deserializeJson(d,missing->output.substr(split+4)));
  REQUIRE(d["utc"].isNull() && d["schedule"]["lightsOn"].isNull() && d["rgbMode"]=="off");
  Wire.failure=false;
  return assertions;
}
