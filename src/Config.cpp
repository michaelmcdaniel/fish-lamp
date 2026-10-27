#include "Config.h"
#include "ZipCodes.h"
#include <ArduinoJson.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {
struct JsonInput {
  const char* data;
  size_t length, position = 0;
  int read() { return position < length ? static_cast<unsigned char>(data[position++]) : -1; }
  size_t readBytes(char* out, size_t count) {
    size_t n = count < length - position ? count : length - position;
    memcpy(out, data + position, n); position += n; return n;
  }
};
class BudgetAllocator : public ArduinoJson::Allocator {
  struct alignas(max_align_t) Header { size_t size; };
  size_t used_ = 0;
  static constexpr size_t Limit = 65536;
 public:
  void* allocate(size_t n) override {
    if (n > Limit - used_ || sizeof(Header) > Limit - used_ - n) return nullptr;
    auto* p = static_cast<Header*>(std::malloc(sizeof(Header) + n));
    if (!p) return nullptr;
    p->size = n + sizeof(Header); used_ += p->size;
    return p + 1;
  }
  void deallocate(void* p) override {
    if (!p) return;
    auto* h = static_cast<Header*>(p) - 1;
    used_ -= h->size; std::free(h);
  }
  void* reallocate(void* p, size_t n) override {
    if (!p) return allocate(n);
    auto* h = static_cast<Header*>(p) - 1;
    size_t old = h->size;
    if (n > Limit - (used_ - old) - sizeof(Header)) return nullptr;
    auto* q = static_cast<Header*>(std::realloc(h, n + sizeof(Header)));
    if (!q) return nullptr;
    q->size = n + sizeof(Header); used_ = used_ - old + q->size;
    return q + 1;
  }
};
bool integer(JsonVariantConst v, int lo, int hi, int& out) {
  if (!v.is<int>()) return false;
  int n = v.as<int>();
  if (n < lo || n > hi) return false;
  out = n; return true;
}
bool pixels(JsonVariantConst v, Pixel* out) {
  if (!v.is<JsonArrayConst>() || v.size() != PixelCount) return false;
  for (unsigned i = 0; i < PixelCount; ++i) {
    auto row = v[i];
    if (!row.is<JsonArrayConst>() || row.size() != 4) return false;
    int c[4];
    for (unsigned j = 0; j < 4; ++j) if (!integer(row[j], 0, 255, c[j])) return false;
    out[i] = {uint8_t(c[0]),uint8_t(c[1]),uint8_t(c[2]),uint8_t(c[3])};
  }
  return true;
}
bool optionalString(JsonVariantConst v, size_t max) {
  return v.isNull() || (v.is<const char*>() && strlen(v.as<const char*>()) <= max);
}
}
bool parseConfig(const char* json, size_t length, Config& destination, int64_t* setupUtc) {
  if (!json || length == 0 || length > MaxConfigBytes || memchr(json, 0, length)) return false;
  while (length && (json[length-1]==' ' || json[length-1]=='\r' ||
                   json[length-1]=='\n' || json[length-1]=='\t')) --length;
  if (!length || json[length-1] != '}') return false;
  BudgetAllocator allocator;
  JsonDocument doc(&allocator);
  JsonInput input{json, length};
  if (deserializeJson(doc, input, DeserializationOption::NestingLimit(4)) ||
      !doc.is<JsonObject>() || input.position != length) return false;
  Config c;
  if (!doc["schema-version"].is<int>() || doc["schema-version"].as<int>() != 1 ||
      !doc["rtc-utc-confirmed"].is<bool>() || !doc["rtc-utc-confirmed"].as<bool>() ||
      !pixels(doc["lights"], c.lights) || !pixels(doc["nightlights"], c.nightlights)) return false;
  if (!doc["latitude"].is<double>() || !doc["longitude"].is<double>()) return false;
  c.latitude = doc["latitude"].as<double>(); c.longitude = doc["longitude"].as<double>();
  if (!std::isfinite(c.latitude) || fabs(c.latitude) > 90 ||
      !std::isfinite(c.longitude) || fabs(c.longitude) > 180) return false;
  if (!integer(doc["timezone-offset-minutes"], -720, 840, c.zone.offsetMinutes) ||
      !doc["timezone-dst"].is<const char*>()) return false;
  const char* dst = doc["timezone-dst"];
  if (strcmp(dst, "none") && strcmp(dst, "us")) return false;
  c.zone.usDst = !strcmp(dst, "us");
  if (!integer(doc["lights-sunrise-offset"], -43200, 43200, c.lightsOffsets.sunrise) ||
      !integer(doc["lights-sunset-offset"], -43200, 43200, c.lightsOffsets.sunset) ||
      !integer(doc["lights-max-offset"], 0, 86400, c.lightsOffsets.max) ||
      !integer(doc["growlight-sunrise-offset"], -43200, 43200, c.growOffsets.sunrise) ||
      !integer(doc["growlight-sunset-offset"], -43200, 43200, c.growOffsets.sunset) ||
      !integer(doc["growlight-max-offset"], 0, 86400, c.growOffsets.max)) return false;
  if (!optionalString(doc["wifi-sid"], 32) || !optionalString(doc["wifi-pwd"], 64) ||
      !optionalString(doc["temperature-url"], 512)) return false;
  const char* ssid = doc["wifi-sid"] | "";
  const char* password = doc["wifi-pwd"] | "";
  // Core supports open networks and WPA passphrases / 64-digit hex PSKs.
  const size_t passwordLength = strlen(password);
  if (passwordLength && (passwordLength < 8 || !*ssid)) return false;
  for (size_t i = 0; i < passwordLength; ++i) {
    unsigned char ch = static_cast<unsigned char>(password[i]);
    if (ch < 32 || ch > 126) return false;
    if (passwordLength == 64 && !((ch >= '0' && ch <= '9') ||
        (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) return false;
  }
  memcpy(c.wifiSsid, ssid, strlen(ssid)+1);
  memcpy(c.wifiPassword, password, passwordLength+1);
  if(!optionalString(doc["zip-code"],5))return false;
  const char* zip=doc["zip-code"] | "";
  if(*zip && (!*ssid || !zipcodes::valid(zip)))return false;
  memcpy(c.zipCode,zip,strlen(zip)+1);
  if (setupUtc) {
    if (!doc["setup-utc"].is<int64_t>()) return false;
    int64_t utc = doc["setup-utc"].as<int64_t>();
    if (utc < clockmath::midnight({2000,1,1}) || utc >= clockmath::midnight({2100,1,1})) return false;
    *setupUtc = utc;
  }
  destination = c;
  return true;
}
bool setConfigLocation(char* json,size_t& length,double latitude,double longitude,const char* source) {
  BudgetAllocator allocator;JsonDocument doc(&allocator);
  if(deserializeJson(doc,static_cast<const char*>(json),length,DeserializationOption::NestingLimit(4)))return false;
  doc["latitude"]=latitude;doc["longitude"]=longitude;doc["location-source"]=source;
  if(doc.overflowed() || measureJson(doc)>MaxConfigBytes)return false;
  length=serializeJson(doc,json,MaxConfigBytes+1);return true;
}
