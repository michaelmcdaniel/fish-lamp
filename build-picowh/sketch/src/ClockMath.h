#line 1 "C:\\Users\\mmcda\\OneDrive\\Code\\pico\\fish-lamp\\src\\ClockMath.h"
#pragma once
#include <stdint.h>

namespace clockmath {
struct Date { int year, month, day; };
struct Zone { int offsetMinutes = 0; bool usDst = false; };
bool leap(int year);
bool valid(Date date);
int64_t midnight(Date date);
Date dateAt(int64_t seconds);
int offsetAt(int64_t utc, Zone zone);
int64_t localToUtc(int64_t local, Zone zone);
bool decodeRtc(const uint8_t* registers, uint8_t status, int64_t& utc);
}
