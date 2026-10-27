#include "ScheduleController.h"
#include <initializer_list>

void ScheduleController::nextTransitions(int64_t utc, bool grow, int64_t& on, int64_t& off) const {
  on=off=0;
  int64_t boundaries[6]; unsigned count=0;
  auto active=[&](int64_t t) {
    for(const auto& d:days_) if((grow ? d.grow : d.lights).contains(t)) return true;
    return false;
  };
  for(const auto& d:days_) {
    const Window w=grow ? d.grow : d.lights;
    if(w.on<w.off) {boundaries[count++]=w.on;boundaries[count++]=w.off;}
  }
  for(unsigned i=0;i<count;++i) {
    const int64_t t=boundaries[i];
    if(t<=utc || active(t-1)==active(t)) continue;
    if(active(t)) {if(!on || t<on)on=t;} else if(!off || t<off)off=t;
  }
}

void ScheduleController::tick(int64_t utc, uint32_t ms, const Config& c) {
  int64_t day = clockmath::midnight(clockmath::dateAt(utc + clockmath::offsetAt(utc, c.zone)));
  if (day != localDay_) {
    localDay_ = day;
    for (int i = 0; i < 3; ++i)
      days_[i] = solarSchedule(clockmath::dateAt(day + (i-1)*86400),
                               c.latitude, c.longitude, c.zone, c.lightsOffsets, c.growOffsets);
    solarValid = days_[1].solarValid;
    lastCalculatedMs = ms;
  }
  dayOn = growOn = false;
  nextLightsBoundary = nextGrowBoundary = 0;
  auto next = [utc](Window w, int64_t& result) {
    if (w.on >= w.off) return;
    for (int64_t boundary : {w.on, w.off})
      if (boundary > utc && (result == 0 || boundary < result)) result = boundary;
  };
  for (auto& d : days_) {
    dayOn |= d.lights.contains(utc); growOn |= d.grow.contains(utc);
    next(d.lights, nextLightsBoundary); next(d.grow, nextGrowBoundary);
  }
  lightsOverride.tick(ms, utc); growOverride.tick(ms, utc);
  if (lightsOverride.active) dayOn = lightsOverride.on;
  if (growOverride.active) growOn = growOverride.on;
  forceOff = lightsOverride.active && lightsOverride.forceOff;
}
