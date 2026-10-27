#include "ConfigManager.h"
#include <LittleFS.h>

static bool load(const char* path, Config& destination) {
  File file = LittleFS.open(path, "r");
  if (!file || !file.size() || file.size() > MaxConfigBytes) return false;
  static char buffer[MaxConfigBytes];
  const size_t length = file.size();
  const size_t read = file.readBytes(buffer, length);
  file.close();
  bool ok = read == length && parseConfig(buffer, length, destination);
  memset(buffer, 0, sizeof(buffer));
  return ok;
}
void ConfigManager::begin() {
  LittleFSConfig options;
  options.setAutoFormat(false);
  LittleFS.setConfig(options);
  if (!LittleFS.begin()) { state = ConfigState::MountFailed; return; }
  mounted_ = true;
  File request = LittleFS.open("/setup.request", "r");
  setupRequested_ = bool(request); request.close();
  state = load("/config.json", config) ? ConfigState::Primary :
          load("/config.bak", config) ? ConfigState::Backup : ConfigState::Invalid;
}

bool ConfigManager::requestSetup() {
  if(setupRequested_) return true;
  if(!mounted_) return false; // Never format storage to record a button press.
  constexpr char marker[]="setup\n";
  File file=LittleFS.open("/setup.request.tmp","w");
  bool written=file && file.write(reinterpret_cast<const uint8_t*>(marker),sizeof(marker)-1)==sizeof(marker)-1;
  file.flush();file.close();
  if(!written) return false;
  file=LittleFS.open("/setup.request.tmp","r");
  if(!file || file.size()!=sizeof(marker)-1) return false;
  for(unsigned i=0;i<sizeof(marker)-1;++i) if(file.read()!=marker[i]) return false;
  file.close();
  if(!LittleFS.rename("/setup.request.tmp","/setup.request")) return false;
  setupRequested_=true;
  return true;
}

bool ConfigManager::clearSetupRequest() {
  if(!setupRequested_) return true;
  if(!mounted_ || !LittleFS.remove("/setup.request")) return false;
  setupRequested_=false;
  return true;
}

bool ConfigManager::save(const char* json, size_t length) {
  Config candidate;
  if (!parseConfig(json, length, candidate)) return false;
  // Formatting is allowed only on explicit setup submission, never at boot.
  if (!mounted_) {
    if (!LittleFS.format() || !LittleFS.begin()) return false;
    mounted_ = true;
  }
  File file = LittleFS.open("/config.tmp", "w");
  if (!file) return false;
  bool written = file.write(reinterpret_cast<const uint8_t*>(json), length) == length;
  file.flush(); file.close();
  if (!written) return false;
  // Verify exact persisted bytes as well as schema before replacing anything.
  file = LittleFS.open("/config.tmp", "r");
  if (!file || file.size() != length) return false;
  for (size_t i = 0; i < length; ++i)
    if (file.read() != static_cast<unsigned char>(json[i])) return false;
  file.close();
  Config verified;
  if (!load("/config.tmp", verified)) return false;
  // LittleFS rename is atomic. A power loss between renames leaves the old
  // valid primary in backup. Never rotate a corrupt primary over a good backup.
  if (load("/config.json", verified) && !LittleFS.rename("/config.json", "/config.bak")) return false;
  if (!LittleFS.rename("/config.tmp", "/config.json")) return false;
  if (!load("/config.json", verified)) return false;
  // A forced setup request survives power cycles until setup saves successfully.
  if(!clearSetupRequest()) return false;
  config = candidate;
  state = ConfigState::Primary;
  return true;
}
