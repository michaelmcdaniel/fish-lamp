#pragma once
#include "ClockMath.h"

struct Offsets { int sunrise = 0, sunset = 0, max = 86400; };
struct Window {
  int64_t on = 0, off = 0;
  bool contains(int64_t now) const { return on < off && now >= on && now < off; }
};
struct ScheduleDay { Window lights, grow; bool solarValid = false; };
ScheduleDay solarSchedule(clockmath::Date date, double latitude, double longitude,
                          clockmath::Zone zone, Offsets lights, Offsets grow);

// Overrides are requests, never hardware writes. Expire at one hour or the
// next schedule boundary, whichever occurs first; force-off overrides night RGB.
struct ManualOverride {
  bool active = false, on = false, forceOff = false;
  uint32_t startedMs = 0;
  int64_t boundary = 0;
  void request(bool value, bool force, uint32_t ms, int64_t nextBoundary);
  void tick(uint32_t ms, int64_t utc);
};
