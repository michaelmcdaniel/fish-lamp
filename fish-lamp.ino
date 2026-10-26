#include <Arduino.h>

#include "src/LightingController.h"
#include "src/TimeService.h"
#include "src/ConfigManager.h"
#include "src/ScheduleController.h"
#include "src/Diagnostics.h"
#include "src/SetupService.h"

LightingController lighting;
TimeService clockService;
ConfigManager settings;
ScheduleController schedule;
Diagnostics diagnostics;
SetupService networking;

void setup() {
  lighting.begin();
  Serial.begin(115200);
  rp2040.wdt_begin(8000);
  settings.begin();
  clockService.begin();
  diagnostics.begin();
  networking.begin(settings);
}

void loop() {
  const uint32_t started = micros();
  const uint32_t now = millis();
  clockService.tick(now);
  if (settings.valid() && clockService.valid() && !networking.restarting()) {
    schedule.tick(clockService.utc(), now, settings.config);
    lighting.apply(schedule.forceOff ? nullptr : schedule.dayOn ?
                   settings.config.lights : settings.config.nightlights, schedule.growOn);
  } else {
    lighting.apply(nullptr, false);
  }
  // Only essential control progress feeds the hardware watchdog.
  rp2040.wdt_reset();
  diagnostics.tick(now, micros() - started, settings, clockService, schedule, lighting, int(networking.state()));
  networking.tick(millis(), settings, clockService);
}
