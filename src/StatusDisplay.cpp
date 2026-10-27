#include "StatusDisplay.h"
#if FISH_LAMP_OLED
#include "ConfigManager.h"
#include "TimeService.h"
#include "ScheduleController.h"
#include "LightingController.h"
#include "SetupService.h"
#include "SensorManager.h"
#include "MockDebug.h"
#include <Wire.h>
#include <cstdio>
#include <cstring>

namespace {
const char* networkName(SetupService::State state) {
  using S = SetupService::State;
  switch(state) {
    case S::Disabled: return "disabled";
    case S::StartAP: return "AP starting";
    case S::AccessPoint: return "setup AP";
    case S::APFailed: return "AP failed";
    case S::Testing: return "testing";
    case S::Restart: return "restarting";
    case S::StationConnecting: return "connecting";
    case S::StationOnline: return "online";
    case S::StationRetry: return "offline/retry";
    case S::Scanning: return "scanning";
  }
  return "unknown";
}
void sensorLine(char (&line)[26], const char* name, const SensorReading& reading,
                const char* unit) {
  if(!reading.valid) snprintf(line,sizeof(line),"%s: -- unavailable",name);
  else snprintf(line,sizeof(line),"%s: %.1f%s%s",name,double(reading.value),unit,
                reading.mocked ? " MOCK" : reading.healthy ? "" : " STALE");
}
}

void StatusDisplay::begin(uint32_t now) {
  // Dedicated I2C1 bus; TimeService exclusively configures RTC I2C0.
  Wire1.setSDA(devices::oledSdaPin);
  Wire1.setSCL(devices::oledSclPin);
  Wire1.begin();
  Wire1.setClock(100000);
  Wire1.setTimeout(5, true);
  oled_.setFont(u8g2_font_5x8_tf);
  oled_.setFontPosBaseline();
  lastMs_ = uptimeMs_ = now;
  uptimeSeconds_ = now/1000; uptimeRemainder_ = uint16_t(now%1000);
  phase_ = Phase::PowerWait;
}

bool StatusDisplay::write(uint8_t control, const uint8_t* bytes, unsigned count) {
  Wire1.beginTransmission(devices::oledAddress);
  bool complete = Wire1.write(control) == 1;
  complete = (Wire1.write(bytes,count) == count) && complete;
  const bool acknowledged = Wire1.endTransmission() == 0;
  if(!complete || !acknowledged) {
    failed_ = true; phase_ = Phase::Off; pending_ = 0;
    mockdebug::log("OLED unavailable at 0x%02x; display disabled until restart\r\n",
                   unsigned(devices::oledAddress));
    return false;
  }
  return true;
}

void StatusDisplay::render(const char (&lines)[8][26], bool all) {
  oled_.clearBuffer();
  for(unsigned row=0;row<8;++row) {
    // 5x8 font has ascent 6 and descent 1: pixels stay in this 8-pixel page.
    oled_.drawStr(0,uint8_t(row*8+6),lines[row]);
    if(all || strcmp(lines[row],previous_[row])) pending_ |= uint8_t(1u<<row);
  }
  memcpy(previous_,lines,sizeof(previous_));
}

void StatusDisplay::sendPart() {
  if(!addressed_) {
    for(row_=0;row_<8 && !(pending_ & (1u<<row_));++row_) {}
    // SH1106's visible 128 columns start at RAM column 2.
    const uint8_t position[]{uint8_t(0xb0 | row_),0x02,0x10};
    if(write(0x00,position,sizeof(position))) { addressed_=true; column_=0; }
    return;
  }
  // One transaction per tick, <= 17 payload bytes, bounded by Wire1's 5ms
  // timeout. Lighting/watchdog progress runs between every chunk.
  if(!write(0x40,oled_.getBufferPtr()+unsigned(row_)*128+column_,16)) return;
  column_ += 16;
  if(column_ == 128) { pending_ &= uint8_t(~(1u<<row_)); addressed_=false; }
}

void StatusDisplay::snapshot(uint32_t now, const ConfigManager& settings,
    const TimeService& clock, const ScheduleController& schedule,
    const LightingController& lights, const SetupService& network,
    const SensorManager& sensors) {
  char lines[8][26]{};
  if(network.signalValid()) snprintf(lines[0],26,"WiFi: online %lddBm",static_cast<long>(network.signalDbm()));
  else if(network.state()==SetupService::State::StationOnline) snprintf(lines[0],26,"WiFi: online --dBm");
  else snprintf(lines[0],26,"WiFi: %s",networkName(network.state()));

  const char* rtc = !devices::rtc ? "MOCK" : clock.rtcHealthy() ? "OK" : "FAIL";
  if(clock.valid()) {
    int64_t seconds = clock.utc();
    if(settings.valid()) seconds += clockmath::offsetAt(seconds,settings.config.zone);
    const unsigned daySeconds = unsigned(seconds % 86400);
    snprintf(lines[1],26,"RTC %s %02u:%02u:%02u %s",rtc,daySeconds/3600,
             daySeconds/60%60,daySeconds%60,settings.valid() ? "L" : "Z");
  } else snprintf(lines[1],26,"RTC %s time unavailable",rtc);
  sensorLine(lines[2],"Water",sensors.waterF,"F");
  sensorLine(lines[3],"Light",sensors.ambientPercent,"%");
  snprintf(lines[4],26,"Grow: %s%s%s",lights.growOn() ? "ON" : "OFF",
           devices::grow ? "" : " MOCK",schedule.growOverride.active ? " MAN" : "");
  const bool active = settings.valid() && clock.valid() && !network.restarting() && !schedule.forceOff;
  const char* mode = !lights.ready() ? "FAIL" : !active ? "OFF" : schedule.dayOn ? "DAY" : "NIGHT";
  snprintf(lines[5],26,"RGB %s%s B:%u-%u%s",mode,schedule.lightsOverride.active ? "*" : "",
           unsigned(lights.minBrightness()),unsigned(lights.maxBrightness()),devices::strip ? "" : " M");
  const unsigned long up = uptimeSeconds_;
  snprintf(lines[6],26,"Up %lud %02lu:%02lu:%02lu",up/86400,up/3600%24,up/60%60,up%60);

  // Record the most recently observed fault transition, retaining it after
  // recovery. Only fixed application messages enter the warning history.
  const bool fault[]{!settings.valid(),devices::rtc && !clock.rtcHealthy(),
    devices::water && !sensors.waterF.healthy,devices::ambient && !sensors.ambientPercent.healthy,
    devices::cpu && !sensors.cpuC.healthy,!lights.ready(),
    settings.valid() && clock.valid() && !schedule.solarValid,
    network.state()==SetupService::State::StationRetry,
    network.state()==SetupService::State::APFailed,settings.state==ConfigState::Backup};
  const char* const message[]{"Settings unavailable","RTC unavailable","Water unavailable",
    "Ambient unavailable","CPU temp unavailable","RGB init failed","Solar unavailable",
    "WiFi disconnected","Setup AP failed","Using backup settings"};
  uint32_t faults=0;
  for(unsigned i=0;i<sizeof(fault)/sizeof(fault[0]);++i) {
    if(!fault[i]) continue;
    faults |= 1u<<i;
    if(!(faults_ & (1u<<i))) {snprintf(lastWarning_,sizeof(lastWarning_),"%s",message[i]);warningSeconds_=uptimeSeconds_;}
  }
  faults_=faults;
  if(strcmp(setupError_,network.lastError())) {
    setupError_=network.lastError();
    if(*setupError_) {snprintf(lastWarning_,sizeof(lastWarning_),"%s",setupError_);warningSeconds_=uptimeSeconds_;}
  }
  if(*lastWarning_) {
    const unsigned length=unsigned(strlen(lastWarning_));
    if(length<=19) snprintf(lines[7],26,"Last: %.19s",lastWarning_);
    else {
      constexpr unsigned width=21;
      static_assert(sizeof(lastWarning_)<=9*width+1,"Warning page numbers must fit one digit");
      const unsigned pages=(length+width-1)/width;
      const unsigned page=uint32_t(uptimeSeconds_-warningSeconds_)/3%pages;
      snprintf(lines[7],26,"%c/%c %.21s",int('1'+page),int('0'+pages),lastWarning_+page*width);
    }
  }
  if(settings.setupRequested() && !*network.lastError()) snprintf(lines[7],26,"Setup on next boot");
  render(lines);
  lastMs_=now;
}

void StatusDisplay::tick(uint32_t now, const ConfigManager& settings, const TimeService& clock,
    const ScheduleController& schedule, const LightingController& lights,
    const SetupService& network, const SensorManager& sensors) {
  // Accumulate uptime across millis wrap; do not derive it from wrapped millis.
  const uint32_t elapsed=now-uptimeMs_;
  uptimeMs_=now;
  const uint64_t ms=uint64_t(elapsed)+uptimeRemainder_;
  uptimeSeconds_+=uint32_t(ms/1000); uptimeRemainder_=uint16_t(ms%1000);
  if(phase_==Phase::Off) return;
  if(phase_==Phase::PowerWait) {
    if(uint32_t(now-lastMs_)<100) return;
    // SH1106: display off, oscillator, mux, offset/start line, DC/DC, orientation,
    // COM layout, contrast, precharge, VCOM, RAM output, normal polarity.
    const uint8_t init[]{0xae,0xd5,0x80,0xa8,0x3f,0xd3,0x00,0x40,0xad,0x8b,
                         0xa1,0xc8,0xda,0x12,0x81,0x80,0xd9,0x22,0xdb,0x35,0xa4,0xa6};
    if(!write(0x00,init,sizeof(init))) return; // ACK verifies configured address.
    const char splash[8][26]{"Fish Lamp","Starting diagnostics..."};
    render(splash,true); phase_=Phase::Splash; return;
  }
  if(pending_) { sendPart(); return; }
  if(!screenOn_) {
    const uint8_t on=0xaf;
    if(write(0x00,&on,1)) {screenOn_=true;lastMs_=now;}
    return;
  }
  if(uint32_t(now-lastMs_) < (phase_==Phase::Splash ? 1000u : 500u)) return;
  phase_=Phase::Live;
  snapshot(now,settings,clock,schedule,lights,network,sensors);
}
#else
// Disabling the display removes the renderer, buffer, dependency and all I2C I/O.
void StatusDisplay::begin(uint32_t) {}
void StatusDisplay::tick(uint32_t, const ConfigManager&, const TimeService&,
    const ScheduleController&, const LightingController&, const SetupService&,
    const SensorManager&) {}
#endif
