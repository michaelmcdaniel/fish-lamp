#pragma once
#include "Devices.h"
#if FISH_LAMP_OLED
#include <U8g2lib.h>
#endif

class ConfigManager;
class TimeService;
class ScheduleController;
class LightingController;
class SetupService;
class SensorManager;

// Optional observer: no sensor acquisition, lamp commands, or network requests.
class StatusDisplay {
 public:
  void begin(uint32_t now);
  void tick(uint32_t now, const ConfigManager& settings, const TimeService& clock,
            const ScheduleController& schedule, const LightingController& lights,
            const SetupService& network, const SensorManager& sensors);
  bool failed() const { return failed_; }
 private:
  bool failed_ = false;
#if FISH_LAMP_OLED
  bool write(uint8_t control, const uint8_t* bytes, unsigned count);
  void sendPart();
  void render(const char (&lines)[8][26], bool all = false);
  void snapshot(uint32_t now, const ConfigManager&, const TimeService&,
                const ScheduleController&, const LightingController&,
                const SetupService&, const SensorManager&);
  // U8g2 supplies its static 1024-byte framebuffer/font renderer only. We own
  // initialization and chunked transport, never call begin()/sendBuffer().
  U8G2_SH1106_128X64_NONAME_F_HW_I2C oled_{U8G2_R0, U8X8_PIN_NONE};
  enum class Phase { Off, PowerWait, Splash, Live };
  Phase phase_ = Phase::Off;
  uint32_t lastMs_ = 0, uptimeMs_ = 0, uptimeSeconds_ = 0, faults_ = 0;
  uint32_t warningSeconds_ = 0;
  uint16_t uptimeRemainder_ = 0;
  uint8_t pending_ = 0, row_ = 0, column_ = 0;
  bool addressed_ = false, screenOn_ = false;
  char previous_[8][26]{}, lastWarning_[128]{};
  const char* setupError_ = "";
#endif
};
