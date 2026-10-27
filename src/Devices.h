#pragma once
#include <Arduino.h>
#include <stdint.h>

// Set to 1 for a Pico with no external gadgets attached. Rebuild and upload.
#ifndef FISH_LAMP_BENCH
#define FISH_LAMP_BENCH 0
#endif
// Each switch can also be overridden by a compiler -D flag (0=mock, 1=real).
#ifndef FISH_LAMP_STRIP
#define FISH_LAMP_STRIP (!FISH_LAMP_BENCH)
#endif
#ifndef FISH_LAMP_GROW
#define FISH_LAMP_GROW (!FISH_LAMP_BENCH)
#endif
#ifndef FISH_LAMP_RTC
#define FISH_LAMP_RTC (!FISH_LAMP_BENCH)
#endif
#ifndef FISH_LAMP_WATER
#define FISH_LAMP_WATER (!FISH_LAMP_BENCH)
#endif
#ifndef FISH_LAMP_AMBIENT
#define FISH_LAMP_AMBIENT (!FISH_LAMP_BENCH)
#endif
#ifndef FISH_LAMP_CPU_TEMP
#define FISH_LAMP_CPU_TEMP 1
#endif
#ifndef FISH_LAMP_STATUS_LED
#define FISH_LAMP_STATUS_LED 1
#endif
#ifndef FISH_LAMP_WIFI
#define FISH_LAMP_WIFI 1
#endif
#ifndef FISH_LAMP_OLED
#define FISH_LAMP_OLED 1
#endif

namespace devices {
// GPIO numbers, except the board-defined status LED and internal ADC channel.
constexpr uint8_t stripPin = 7; // 7 ACTIVE
constexpr uint8_t growPin = 8; // 8 ACTIVE - HIGH = on, LOW = off.
constexpr uint8_t waterPin = 26; // 26 ACTIVE
constexpr uint8_t ambientPin = 27;  // 27 ACTIVE
constexpr uint8_t rtcSdaPin = 16; // 16 ACTIVE
constexpr uint8_t rtcSclPin = 17; // 17 ACTIVE
constexpr uint8_t oledSdaPin = 2; // Wire1; RTC stays on Wire at GPIO 16/17.
constexpr uint8_t oledSclPin = 3;
constexpr uint8_t oledAddress = 0x3c; // 7-bit address; change to 0x3d if required.

constexpr auto statusLedPin = LED_BUILTIN; // Pico 2 W Wi-Fi-chip LED.
constexpr uint8_t cpuTempAdcChannel = 4; // Internal ADC, NOT GPIO 4; use analogReadTemp().
// Onboard Wi-Fi uses the core's board wiring; no application GPIO declaration.
static_assert((FISH_LAMP_BENCH==0 || FISH_LAMP_BENCH==1) &&
              (FISH_LAMP_STRIP==0 || FISH_LAMP_STRIP==1) &&
              (FISH_LAMP_GROW==0 || FISH_LAMP_GROW==1) &&
              (FISH_LAMP_RTC==0 || FISH_LAMP_RTC==1) &&
              (FISH_LAMP_WATER==0 || FISH_LAMP_WATER==1) &&
              (FISH_LAMP_AMBIENT==0 || FISH_LAMP_AMBIENT==1) &&
              (FISH_LAMP_CPU_TEMP==0 || FISH_LAMP_CPU_TEMP==1) &&
              (FISH_LAMP_STATUS_LED==0 || FISH_LAMP_STATUS_LED==1) &&
              (FISH_LAMP_WIFI==0 || FISH_LAMP_WIFI==1) &&
              (FISH_LAMP_OLED==0 || FISH_LAMP_OLED==1), "Device switches must be 0 or 1");
constexpr bool oled=FISH_LAMP_OLED;
#if FISH_LAMP_OLED
static_assert(oledSdaPin % 4 == 2 && oledSclPin == oledSdaPin + 1,
              "OLED requires a Wire1 SDA/SCL pair; do not remap the RTC bus");
static_assert(oledAddress == 0x3c || oledAddress == 0x3d, "Check the SH1106 I2C address");
#endif
constexpr bool strip=FISH_LAMP_STRIP, grow=FISH_LAMP_GROW, rtc=FISH_LAMP_RTC;
constexpr bool water=FISH_LAMP_WATER, ambient=FISH_LAMP_AMBIENT;
constexpr bool cpu=FISH_LAMP_CPU_TEMP, status=FISH_LAMP_STATUS_LED, wifi=FISH_LAMP_WIFI;
constexpr float mockWaterF=77.0f, mockAmbientPercent=50.0f, mockCpuC=30.0f;
// 2026-01-01 12:00 UTC, then advances using millis(). Setup can set RAM time.
constexpr int64_t mockRtcUtc=1767268800;
}
