#pragma once
#include "SetupService.h"
#include "ScheduleController.h"
#include "SensorManager.h"
#include "LightingController.h"

// One client, one queued connection, fixed response storage and bounded tick I/O.
class WebService {
 public:
  void tick(uint32_t now, const ConfigManager& settings, const TimeService& clock,
            const ScheduleController& schedule, const LightingController& lighting,
            const SetupService& network, const SensorManager& sensors);
 private:
  void close();
  void reply(int code, const char* type, const char* body, uint32_t now);
  bool snapshot(uint32_t now, const ConfigManager&, const TimeService&,
                const ScheduleController&, const LightingController&,
                const SetupService&, const SensorManager&);
  WiFiServer server_{80};
  WiFiClient client_;
  SetupRequest request_;
  bool running_=false, active_=false, responding_=false;
  uint32_t accepted_=0, progress_=0;
  char json_[10000]{}, header_[256]{};
  const char* body_=nullptr;
  size_t headerLength_=0, bodyLength_=0, sent_=0;
};
