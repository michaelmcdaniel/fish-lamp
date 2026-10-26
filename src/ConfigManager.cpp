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
  state = load("/config.json", config) ? ConfigState::Primary :
          load("/config.bak", config) ? ConfigState::Backup : ConfigState::Invalid;
}
