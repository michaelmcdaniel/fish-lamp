#include "WebService.h"
#include "DashboardPage.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
void reading(JsonObject out, const SensorReading& r, uint32_t now) {
  if(r.valid && std::isfinite(r.value)) out["value"]=r.value;
  else out["value"]=nullptr;
  out["valid"]=r.valid; out["healthy"]=r.healthy; out["mocked"]=r.mocked;
  if(r.valid) out["ageMs"]=uint32_t(now-r.lastSuccessMs); else out["ageMs"]=nullptr;
}
void pixels(JsonArray out, const Pixel* values) {
  for(unsigned i=0;i<PixelCount;++i) {
    auto row=out.add<JsonArray>(); const auto& p=values[i];
    row.add(p.r);row.add(p.g);row.add(p.b);row.add(p.brightness);
  }
}
int64_t expiry(const ManualOverride& o, uint32_t ms, int64_t utc) {
  if(!o.active) return 0;
  const uint32_t elapsed=uint32_t(ms-o.startedMs);
  const int64_t end=utc+(elapsed>=3600000 ? 0 : (3600000-elapsed+999)/1000);
  return o.boundary>0 ? std::min(end,o.boundary) : end;
}
}
bool WebService::snapshot(uint32_t now, const ConfigManager& settings, const TimeService& clock,
    const ScheduleController& schedule, const LightingController& lighting,
    const SetupService& network, const SensorManager& sensors) {
  JsonDocument d;
  const bool ready=settings.valid() && clock.valid() && !network.restarting();
  d["uptimeMs"]=now; d["configValid"]=settings.valid(); d["timeValid"]=clock.valid();
  if(clock.valid()) d["utc"]=clock.utc(); else d["utc"]=nullptr;
  d["rtcHealthy"]=clock.rtcHealthy(); d["rtcMocked"]=!devices::rtc;
  d["rtcAgeMs"]=uint32_t(now-clock.lastRtcMs);
  d["timezoneOffsetSeconds"]=clock.valid() ? clockmath::offsetAt(clock.utc(),settings.config.zone) : 0;
  auto zone=d["zone"].to<JsonObject>();zone["offsetMinutes"]=settings.config.zone.offsetMinutes;zone["usDst"]=settings.config.zone.usDst;
  d["lightsOn"]=ready && schedule.dayOn && !schedule.forceOff;
  d["growlightOn"]=lighting.growOn(); d["lightsForceOff"]=schedule.forceOff;
  d["stripReady"]=lighting.ready();
  d["rgbMode"]=!ready || schedule.forceOff ? "off" : schedule.dayOn ? "day" : "night";
  d["lightsDelay"]=ready ? expiry(schedule.lightsOverride,now,clock.utc()) : 0;
  d["growlightDelay"]=ready ? expiry(schedule.growOverride,now,clock.utc()) : 0;
  auto plan=d["schedule"].to<JsonObject>();
  plan["valid"]=ready && schedule.solarValid;
  int64_t on=0,off=0;
  if(ready) schedule.nextTransitions(clock.utc(),false,on,off);
  if(on) plan["lightsOn"]=on; else plan["lightsOn"]=nullptr;
  if(off) plan["lightsOff"]=off; else plan["lightsOff"]=nullptr;
  on=off=0;
  if(ready) schedule.nextTransitions(clock.utc(),true,on,off);
  if(on) plan["growlightOn"]=on; else plan["growlightOn"]=nullptr;
  if(off) plan["growlightOff"]=off; else plan["growlightOff"]=nullptr;
  auto offsets=[&](const char* name, Offsets o) {
    auto a=d[name].to<JsonObject>();a["sunriseSeconds"]=o.sunrise;a["sunsetSeconds"]=o.sunset;a["absoluteOffSeconds"]=o.max;
  };
  offsets("lightsOffsets",settings.config.lightsOffsets);offsets("growlightOffsets",settings.config.growOffsets);
  pixels(d["lights"].to<JsonArray>(),settings.config.lights);
  pixels(d["nightlights"].to<JsonArray>(),settings.config.nightlights);
  auto s=d["sensors"].to<JsonObject>();
  reading(s["waterF"].to<JsonObject>(),sensors.waterF,now);
  reading(s["cpuC"].to<JsonObject>(),sensors.cpuC,now);
  reading(s["ambientPercent"].to<JsonObject>(),sensors.ambientPercent,now);
  d["waterTemp"]=s["waterF"]["value"]; d["ambient"]=s["ambientPercent"]["value"];
  auto w=d["wifi"].to<JsonObject>();w["connected"]=network.connected();
  w["ssid"]=network.ssid();w["ip"]=network.ipAddress();
  if(network.signalValid()) w["rssiDbm"]=network.signalDbm();else w["rssiDbm"]=nullptr;
  if(d.overflowed() || measureJson(d)>=sizeof(json_)) return false;
  serializeJson(d,json_,sizeof(json_));return true;
}
void WebService::close() {
  if(active_)client_.stop(1);
  client_=WiFiClient();active_=responding_=false;
}
void WebService::reply(int code,const char* type,const char* body,uint32_t now) {
  body_=body;bodyLength_=strlen(body);sent_=0;
  headerLength_=size_t(snprintf(header_,sizeof(header_),
    "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n\r\n",
    code,code==200 ? "OK" : "Error",type,unsigned(bodyLength_)));
  responding_=true;progress_=now;
}
void WebService::tick(uint32_t now,const ConfigManager& settings,const TimeService& clock,
    const ScheduleController& schedule,const LightingController& lighting,
    const SetupService& network,const SensorManager& sensors) {
  const bool online=network.state()==SetupService::State::StationOnline && network.connected();
  if(!online) {close();if(running_)server_.end();running_=false;return;}
  if(!running_) {server_.begin(80,1);running_=bool(server_);if(!running_)return;}
  if(!active_) {
    client_=server_.accept();if(!client_)return;
    active_=true;client_.setTimeout(5);client_.setNoDelay(true);
    request_.reset(true);accepted_=progress_=now;
  }
  if(uint32_t(now-accepted_)>=10000 || uint32_t(now-progress_)>=2000) {close();return;}
  if(responding_) {
    const bool header=sent_<headerLength_;
    const size_t left=header ? headerLength_-sent_ : headerLength_+bodyLength_-sent_;
    const char* data=header ? header_+sent_ : body_+sent_-headerLength_;
    const size_t count=std::min(size_t(256),std::min(left,size_t(std::max(0,client_.availableForWrite()))));
    if(count) {const size_t n=client_.write(reinterpret_cast<const uint8_t*>(data),count);sent_+=n;if(n)progress_=now;}
    if(sent_==headerLength_+bodyLength_ && uint32_t(now-progress_)>=250)close();
    return;
  }
  for(unsigned i=0;i<256 && client_.available()>0 && !request_.complete && !request_.error;++i) {
    request_.feed(char(client_.read()));progress_=now;
  }
  if(request_.error)reply(request_.error,"text/plain","Malformed or unsupported request",now);
  else if(request_.complete) {
    if(request_.route==SetupRequest::Route::Page)reply(200,"text/html; charset=utf-8",DashboardPage,now);
    else if(request_.route==SetupRequest::Route::Status) {
      if(snapshot(now,settings,clock,schedule,lighting,network,sensors))reply(200,"application/json",json_,now);
      else reply(503,"text/plain","Snapshot unavailable",now);
    } else reply(404,"text/plain","Not found",now);
  } else if(!client_.connected())close();
}
