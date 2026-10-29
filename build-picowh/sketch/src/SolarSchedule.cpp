#line 1 "C:\\Users\\mmcda\\OneDrive\\Code\\pico\\fish-lamp\\src\\SolarSchedule.cpp"
#include "SolarSchedule.h"
#include <cmath>
#include <algorithm>

ScheduleDay solarSchedule(clockmath::Date date, double lat, double lon,
                          clockmath::Zone zone, Offsets lights, Offsets grow) {
  ScheduleDay out;
  if (!clockmath::valid(date) || !std::isfinite(lat) || !std::isfinite(lon) ||
      lat < -90 || lat > 90 || lon < -180 || lon > 180) return out;
  constexpr double pi = 3.14159265358979323846;
  const int64_t day = clockmath::midnight(date);
  const int ordinal = int((day - clockmath::midnight({date.year,1,1})) / 86400) + 1;
  // NOAA fractional-year approximation, evaluated at local noon.
  const double gamma = 2 * pi / (clockmath::leap(date.year) ? 366 : 365) * (ordinal - 1);
  const double eq = 229.18 * (0.000075 + 0.001868*cos(gamma) - 0.032077*sin(gamma)
                            - 0.014615*cos(2*gamma) - 0.040849*sin(2*gamma));
  const double dec = 0.006918 - 0.399912*cos(gamma) + 0.070257*sin(gamma)
                     - 0.006758*cos(2*gamma) + 0.000907*sin(2*gamma)
                     - 0.002697*cos(3*gamma) + 0.00148*sin(3*gamma);
  const double rad = lat * pi / 180;
  const double denominator = cos(rad) * cos(dec);
  if (fabs(denominator) < 1e-12) return out;
  const double angle = (cos(90.833*pi/180) - sin(rad)*sin(dec)) / denominator;
  // No sunrise/sunset at polar extremes: deliberate fail-closed day/grow state.
  if (angle < -1 || angle > 1) return out;
  const double minutes = acos(angle) * 180 / pi * 4;
  int64_t noon = day + int64_t(llround((720 - 4*lon - eq) * 60));
  const int64_t localNoon = clockmath::localToUtc(day + 43200, zone);
  while (noon - localNoon > 43200) noon -= 86400;
  while (localNoon - noon > 43200) noon += 86400;
  const int64_t rise = noon - int64_t(llround(minutes * 60));
  const int64_t set = noon + int64_t(llround(minutes * 60));
  auto window = [&](Offsets o) {
    return Window{rise + o.sunrise,
      std::min(set + o.sunset, clockmath::localToUtc(day + o.max, zone))};
  };
  out.lights = window(lights);
  out.grow = window(grow);
  out.solarValid = true;
  return out;
}
void ManualOverride::request(bool value, bool force, uint32_t ms, int64_t nextBoundary) {
  active = true; on = value; forceOff = force; startedMs = ms; boundary = nextBoundary;
}
void ManualOverride::tick(uint32_t ms, int64_t utc) {
  if (active && (uint32_t(ms - startedMs) >= 3600000 ||
                 (boundary > 0 && utc >= boundary))) active = false;
}
