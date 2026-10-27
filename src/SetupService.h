#pragma once
#include "ConfigManager.h"
#include "TimeService.h"
#include "SetupRequest.h"
#include "NetworkScan.h"
#include <WiFi.h>
#include <DNSServer.h>

class SetupService {
 public:
  enum class State { Disabled, StartAP, AccessPoint, APFailed, Testing, Restart,
                     StationConnecting, StationOnline, StationRetry, Scanning };
  void begin(const ConfigManager& settings);
  void tick(uint32_t now, ConfigManager& settings, TimeService& clock);
  State state() const { return state_; }
  bool restarting() const { return state_ == State::Restart; }
  bool signalValid() const { return state_ == State::StationOnline && signalValid_; }
  int32_t signalDbm() const { return signalDbm_; }
  const char* lastError() const { return lastError_; } // Fixed diagnostic messages only.
 private:
  void startAP(uint32_t now);
  void closeClient();
  void reply(int code, const char* type, const char* body, uint32_t now);
  void handle(uint32_t now, ConfigManager& settings, TimeService& clock);
  void commit(uint32_t now, ConfigManager& settings, TimeService& clock);
  void fail(const char* message);
  State state_ = State::Disabled;
  WiFiServer server_{80};
  DNSServer dns_;
  bool dnsRunning_=false;
  WiFiClient client_;
  SetupRequest request_;
  Config candidate_;
  NetworkScan networks_;
  int64_t candidateUtc_ = 0;
  uint32_t stateMs_ = 0, acceptedMs_ = 0, progressMs_ = 0, submittedMs_ = 0;
  bool clientActive_ = false, responding_ = false, testAfterResponse_ = false;
  char token_[33]{}, session_[256]{}, header_[320]{};
  const char* response_ = nullptr;
  const char* lastError_ = "";
  bool signalValid_ = false;
  int32_t signalDbm_ = 0;
  uint32_t signalMs_ = 0;
  size_t headerLength_ = 0, responseLength_ = 0, sent_ = 0;
};
