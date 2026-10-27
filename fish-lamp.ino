#include <Arduino.h>

#include "src/LightingController.h"
#include "src/TimeService.h"
#include "src/ConfigManager.h"
#include "src/ScheduleController.h"
#include "src/Diagnostics.h"
#include "src/SetupService.h"
#include "src/SensorManager.h"
#include "src/StatusLed.h"
#include "src/StatusDisplay.h"
#include "src/SetupButton.h"
#include "src/WebService.h"

LightingController lighting;
TimeService clockService;
ConfigManager settings;
ScheduleController schedule;
Diagnostics diagnostics;
SetupService networking;
SensorManager sensors;
StatusLed statusLed;
StatusDisplay statusDisplay;
SetupButton setupButton;
WebService web;

void setup() {
  lighting.begin();
  Serial.begin(115200);
  rp2040.wdt_begin(8000);
  settings.begin();
  clockService.begin();
  sensors.begin();
  statusLed.begin();
  diagnostics.begin();
  statusDisplay.begin(millis());
  networking.begin(settings);
}

void loop() {
  const uint32_t started = micros();
  const uint32_t now = millis();
  clockService.tick(now);
  sensors.tick(now);
  statusLed.tick(now,networking.connecting(),networking.connected());
  if (settings.valid() && clockService.valid() && !networking.restarting()) {
    schedule.tick(clockService.utc(), now, settings.config);
    lighting.apply(schedule.forceOff ? nullptr : schedule.dayOn ?
                   settings.config.lights : settings.config.nightlights, schedule.growOn);
  } else {
    lighting.apply(nullptr, false);
  }
  // Only essential control progress feeds the hardware watchdog.
  rp2040.wdt_reset();
  const uint32_t controlUs = micros() - started;
  setupButton.tick(millis(), settings);
  // Observe this applied control snapshot before Serial/setup can change it.
  statusDisplay.tick(millis(), settings, clockService, schedule, lighting, networking, sensors);
  diagnostics.tick(now, controlUs, settings, clockService, schedule, lighting, int(networking.state()), sensors);
  networking.tick(millis(), settings, clockService);
  web.tick(millis(), settings, clockService, schedule, lighting, networking, sensors);
}
