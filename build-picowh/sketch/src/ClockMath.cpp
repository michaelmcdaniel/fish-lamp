#line 1 "C:\\Users\\mmcda\\OneDrive\\Code\\pico\\fish-lamp\\src\\ClockMath.cpp"
#include "ClockMath.h"

namespace clockmath {
bool leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }
static int daysInMonth(int y, int m) {
  static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  return days[m - 1] + (m == 2 && leap(y));
}
bool valid(Date d) {
  return d.year >= 2000 && d.year <= 2099 && d.month >= 1 && d.month <= 12 &&
         d.day >= 1 && d.day <= daysInMonth(d.year, d.month);
}
int64_t midnight(Date d) {
  int days = 0;
  for (int y = 1970; y < d.year; ++y) days += leap(y) ? 366 : 365;
  for (int m = 1; m < d.month; ++m) days += daysInMonth(d.year, m);
  return int64_t(days + d.day - 1) * 86400;
}
Date dateAt(int64_t seconds) {
  int64_t days = seconds / 86400;
  Date d{1970, 1, 1};
  while (days >= (leap(d.year) ? 366 : 365)) days -= leap(d.year++) ? 366 : 365;
  while (days >= daysInMonth(d.year, d.month)) days -= daysInMonth(d.year, d.month++);
  d.day += int(days);
  return d;
}
static int sunday(int y, int m) {
  return 1 + (7 - int((midnight({y,m,1}) / 86400 + 4) % 7)) % 7;
}
int offsetAt(int64_t utc, Zone z) {
  int base = z.offsetMinutes * 60;
  if (!z.usDst) return base;
  int y = dateAt(utc + base).year;
  // Explicitly supports current US rules, 2007 onward; not an IANA database.
  if (y < 2007) return base;
  int64_t start = midnight({y,3,sunday(y,3) + 7}) + 7200 - base;
  int64_t end = midnight({y,11,sunday(y,11)}) + 7200 - base - 3600;
  return base + ((utc >= start && utc < end) ? 3600 : 0);
}
int64_t localToUtc(int64_t local, Zone z) {
  int64_t standard = local - z.offsetMinutes * 60;
  int64_t daylight = standard - 3600;
  // A repeated wall time chooses its earlier occurrence. A nonexistent wall
  // time moves forward by the gap, e.g. 02:30 -> 03:30 in spring.
  if (z.usDst && daylight + offsetAt(daylight, z) == local) return daylight;
  return standard;
}
static bool bcd(uint8_t v) { return (v & 15) <= 9 && (v >> 4) <= 9; }
static int decimal(uint8_t v) { return (v >> 4) * 10 + (v & 15); }
bool decodeRtc(const uint8_t* r, uint8_t status, int64_t& utc) {
  if ((status & 0x80) || (r[0] & 0x80) || (r[1] & 0x80) ||
      (r[2] & 0x80) || (r[4] & 0xc0) || (r[5] & 0xe0)) return false;
  uint8_t hour = r[2] & ((r[2] & 0x40) ? 0x1f : 0x3f);
  if (!bcd(r[0]) || !bcd(r[1]) || !bcd(hour) || !bcd(r[4]) ||
      !bcd(r[5]) || !bcd(r[6]) || r[3] < 1 || r[3] > 7) return false;
  int h = decimal(hour);
  if (r[2] & 0x40) {
    if (h < 1 || h > 12) return false;
    h = h % 12 + ((r[2] & 0x20) ? 12 : 0);
  }
  Date d{2000 + decimal(r[6]), decimal(r[5]), decimal(r[4])};
  if (!valid(d) || h > 23 || decimal(r[1]) > 59 || decimal(r[0]) > 59) return false;
  utc = midnight(d) + h * 3600 + decimal(r[1]) * 60 + decimal(r[0]);
  return true;
}
}
