#include "SetupService.h"
#include "SetupPage.h"
#include "Devices.h"
#include "MockDebug.h"
#include "ZipCodes.h"
#include <Arduino.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

void SetupService::begin(const ConfigManager& settings) {
  if(!devices::wifi) { state_=State::Disabled; mockdebug::log("mock WiFi: disconnected; setup unavailable\r\n"); return; }
  WiFi.setTimeout(1000);
  if (!settings.valid() || settings.setupRequested()) {
    WiFi.end(); networks_.begin(millis()); state_=State::Scanning;
  }
  else if (*settings.config.wifiSsid) {
    startStation(settings.config,State::StationConnecting);
  }
}
void SetupService::startStation(const Config& candidate,State nextState) {
  connected_=false;stationIp_[0]=0;
  memcpy(stationSsid_,candidate.wifiSsid,sizeof(stationSsid_));
  dns_.stop(); dnsRunning_=false;
  // In Arduino-Pico 6.0/6.3, end() only disconnects STA. In AP mode,
  // beginNoBlock() otherwise starts another AP instead of joining the router.
  WiFi.disconnectAP();
  WiFi.end();
  WiFi.mode(WIFI_STA);
  const IPAddress automatic(0,0,0,0);
  WiFi.config(automatic,automatic,automatic,automatic); // Fresh DHCP, no AP address.
  const int result=WiFi.beginNoBlock(candidate.wifiSsid,*candidate.wifiPassword ? candidate.wifiPassword : nullptr);
  state_=nextState; stateMs_=millis(); signalValid_=false;
  mockdebug::log("WiFi station start: status=%d mode=%d\r\n",result,int(WiFi.getMode()));
}
void SetupService::cacheNetwork(uint32_t now) {
  const bool fresh=!connected_;connected_=true;
  if(!devices::oled)return;
  if(fresh || uint32_t(now-signalMs_)>=1000) {
    signalDbm_=WiFi.RSSI();signalMs_=now;
    signalValid_=signalDbm_>=-127 && signalDbm_<0;
    const auto ip=WiFi.localIP();
    snprintf(stationIp_,sizeof(stationIp_),"%u.%u.%u.%u",unsigned(ip[0]),unsigned(ip[1]),unsigned(ip[2]),unsigned(ip[3]));
  }
}
bool SetupService::storeLocation(double latitude,double longitude,const char* source) {
  if(!setConfigLocation(request_.body,request_.bodyLength,latitude,longitude,source))return false;
  candidate_.latitude=latitude;candidate_.longitude=longitude;return true;
}
void SetupService::closeClient() {
  if (clientActive_) client_.stop(1); // Default stop can wait seconds for ACKs.
  client_ = WiFiClient(); clientActive_ = responding_ = false;
}
void SetupService::startAP(uint32_t now) {
  zipLookup_.cancel();connected_=false;
  dns_.stop(); dnsRunning_=false;
  WiFi.disconnectAP();
  WiFi.end(); // Never depend on simultaneous station + AP operation.
  // softAPConfig() configures STA instead if mode is not explicitly AP yet.
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0));
  stateMs_ = now;
  // Open setup network, explicitly requested by the owner. No AP password.
  if (!WiFi.softAP("FishLamp-Setup")) { state_=State::APFailed; return; }
  for (unsigned i=0;i<4;++i) snprintf(token_+i*8,9,"%08lx",static_cast<unsigned long>(rp2040.hwrand32()));
  server_.begin(80,1);
  if (!server_) { WiFi.end(); state_=State::APFailed; return; }
  dns_.setTTL(0);
  dnsRunning_=dns_.start(53,"*",IPAddress(192,168,4,1));
  if(!dnsRunning_) mockdebug::log("Setup DNS unavailable; open http://192.168.4.1/ manually\r\n");
  state_=State::AccessPoint;
}
void SetupService::reply(int code, const char* type, const char* body, uint32_t now) {
  const char* reason = code==200 ? "OK" : code==202 ? "Accepted" : code==302 ? "Found" : "Error";
  response_=body; responseLength_=strlen(body); sent_=0;
  headerLength_=size_t(snprintf(header_,sizeof(header_),
    "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n%s\r\n",
    code,reason,type,unsigned(responseLength_),code==302 ? "Location: http://192.168.4.1/\r\n" : ""));
  responding_=true; acceptedMs_=progressMs_=now;
}
void SetupService::fail(const char* message) {
  lastError_=message; testAfterResponse_=false;
  mockdebug::log("Setup failed: %s\r\n",message);
  candidate_=Config{}; request_.reset();
  state_=State::StartAP;
}
void SetupService::commit(uint32_t now, ConfigManager& settings, TimeService& clock) {
  if (!clock.setUtc(candidateUtc_ + uint32_t(now-submittedMs_)/1000, now)) {
    lastError_="RTC initialization failed. Check the DS3231 wiring and retry. Settings were not saved.";
    return;
  }
  if (!settings.save(request_.body,request_.bodyLength)) {
    lastError_="Storage verification failed. Settings were not confirmed saved; retry setup.";
    return;
  }
  state_=State::Restart; stateMs_=millis(); lastError_="";
  memset(candidate_.wifiPassword,0,sizeof(candidate_.wifiPassword));
  request_.reset();
}
void SetupService::handle(uint32_t now, ConfigManager& settings, TimeService& clock) {
  using Route=SetupRequest::Route;
  if (request_.route==Route::Page) reply(200,"text/html; charset=utf-8",SetupPage,now);
  else if(request_.route==Route::Redirect) reply(302,"text/plain","Open http://192.168.4.1/ to configure Fish Lamp.",now);
  else if (request_.route==Route::Networks) reply(200,"application/json",networks_.json(),now);
  else if (request_.route==Route::Session) {
    snprintf(session_,sizeof(session_),"{\"token\":\"%s\",\"error\":\"%s\"}",token_,lastError_);
    reply(200,"application/json",session_,now);
  } else if (request_.route==Route::Configure) {
    if (strcmp(request_.token,token_)) { reply(403,"text/plain","Setup session expired. Reload this page.",now); return; }
    if (!parseConfig(request_.body,request_.bodyLength,candidate_,&candidateUtc_)) {
      reply(400,"text/plain","Invalid settings. Check location, timezone, Wi-Fi password, offsets and 85 LED entries.",now); return;
    }
    submittedMs_=now;
    needsZipLookup_=false;
    if(*candidate_.wifiSsid && *candidate_.zipCode) {
      double latitude,longitude;
      const bool known=zipcodes::lookup(candidate_.zipCode,latitude,longitude);
      if(!known){latitude=zipcodes::FallbackLatitude;longitude=zipcodes::FallbackLongitude;}
      if(!storeLocation(latitude,longitude,known ? "compiled-zip" : "fallback")) {
        reply(400,"text/plain","Location settings exceed configuration capacity.",now);return;
      }
      needsZipLookup_=!known;
    }
    if (*candidate_.wifiSsid) {
      testAfterResponse_=true;
      reply(202,"text/plain","Settings received. The setup network will disconnect while the lamp tests home Wi-Fi (up to 20 seconds), then resolves location (up to 6 seconds). On success it saves and restarts. If FishLamp-Setup returns, reconnect and reload this page for the error; settings were not confirmed saved.",now);
    } else {
      commit(now,settings,clock);
      if (state_==State::Restart) reply(200,"text/plain",devices::rtc ?
        "Settings saved and clock verified. The lamp will restart in a few seconds. Offline lighting is ready." :
        "Settings saved. Mock RTC accepted the time in RAM only; restart restores the mock starting time. The lamp will restart in a few seconds.",millis());
      else {mockdebug::log("Setup failed: %s\r\n",lastError_);reply(503,"text/plain",lastError_,millis());}
    }
  } else reply(404,"text/plain","Not found. Open http://192.168.4.1/",now);
}
void SetupService::tick(uint32_t now, ConfigManager& settings, TimeService& clock) {
  if(!devices::wifi) return;
  if(state_==State::Scanning) {
    if(!networks_.tick(now)) return;
    state_=State::StartAP;
  }
  if (state_==State::StartAP) { startAP(now); return; }
  if (state_==State::APFailed) {
    if (uint32_t(now-stateMs_)>=60000) state_=State::StartAP;
    return;
  }
  if (state_==State::StationConnecting || state_==State::StationOnline) {
    if (WiFi.getMode()==WIFI_STA && WiFi.status()==WL_CONNECTED) {
      state_=State::StationOnline;
      cacheNetwork(now);
    }
    else if (state_==State::StationOnline || uint32_t(now-stateMs_)>=20000) {
      WiFi.end(); state_=State::StationRetry; stateMs_=now; signalValid_=false;connected_=false;
    }
    return;
  }
  if (state_==State::StationRetry) {
    if (uint32_t(now-stateMs_)>=60000) {
      startStation(settings.config,State::StationConnecting);
    }
    return;
  }
  if (state_==State::Testing) {
    if (WiFi.getMode()==WIFI_STA && WiFi.status()==WL_CONNECTED) {
      cacheNetwork(now);
      if(needsZipLookup_){zipLookup_.begin(candidate_.zipCode,now);state_=State::ResolvingLocation;}
      else {commit(now,settings,clock);if(state_!=State::Restart)fail(lastError_);}
    } else if (uint32_t(now-stateMs_)>=20000) {
      const int link=cyw43_wifi_link_status(&cyw43_state,CYW43_ITF_STA);
      mockdebug::log("WiFi test failed: status=%d link=%d mode=%d\r\n",int(WiFi.status()),link,int(WiFi.getMode()));
      if(link==CYW43_LINK_BADAUTH) fail("Home Wi-Fi authentication failed. Check password and router security. Settings were not saved.");
      else if(link==CYW43_LINK_NONET) fail("Home Wi-Fi network not found on 2.4 GHz. Check SSID and band. Settings were not saved.");
      else if(link==CYW43_LINK_JOIN) fail("Wi-Fi joined but DHCP did not provide an address. Check router DHCP. Settings were not saved.");
      else fail("Could not connect to home Wi-Fi. Check the network and password. Settings were not saved.");
    }
    return;
  }
  if(state_==State::ResolvingLocation) {
    if(WiFi.getMode()!=WIFI_STA || WiFi.status()!=WL_CONNECTED) {
      zipLookup_.cancel();connected_=false; // Keep fallback without another join attempt.
    } else {cacheNetwork(now);if(!zipLookup_.tick(now))return;}
    if(zipLookup_.found() && !storeLocation(zipLookup_.latitude(),zipLookup_.longitude(),"api-zip")) {
      fail("Could not store resolved location. Settings were not saved.");return;
    }
    if(!zipLookup_.found())mockdebug::log("ZIP lookup unavailable; using configured fallback coordinates\r\n");
    needsZipLookup_=false;commit(now,settings,clock);if(state_!=State::Restart)fail(lastError_);
    return;
  }
  if (state_==State::Restart && uint32_t(now-stateMs_)>=4000) {
    dns_.stop(); dnsRunning_=false;
    closeClient(); rp2040.reboot(); return;
  }
  if (state_!=State::AccessPoint && state_!=State::Restart) return;
  // The core handles one packet (maximum 512 bytes) per call, without forwarding.
  if(dnsRunning_) dns_.processNextRequest();
  // Keep the single queued connection until the active response closes. Browsers
  // may request /session or /networks before the preceding socket has closed.
  // The installed lwIP enables TCP_LISTEN_BACKLOG; begin(80,1) bounds this queue.
  if (!clientActive_ && state_==State::AccessPoint && !testAfterResponse_) {
    client_=server_.accept();
    if (client_) {
      clientActive_=true; client_.setTimeout(5); client_.setNoDelay(true);
      request_.reset(); acceptedMs_=progressMs_=now;
    }
  }
  if (clientActive_) {
    if (uint32_t(now-acceptedMs_)>=15000 || uint32_t(now-progressMs_)>=2000) closeClient();
    else if (responding_) {
      size_t remaining = sent_<headerLength_ ? headerLength_-sent_ : headerLength_+responseLength_-sent_;
      const char* data = sent_<headerLength_ ? header_+sent_ : response_+sent_-headerLength_;
      size_t count = std::min(size_t(256),std::min(remaining,size_t(std::max(0,client_.availableForWrite()))));
      if (count) { size_t n=client_.write(reinterpret_cast<const uint8_t*>(data),count); sent_+=n; if(n) progressMs_=now; }
      if (sent_==headerLength_+responseLength_ && uint32_t(now-progressMs_)>=250) closeClient();
    } else {
      for (unsigned i=0;i<256 && client_.available()>0 && !request_.complete && !request_.error;++i) {
        request_.feed(char(client_.read())); progressMs_=now;
      }
      if (request_.error) reply(request_.error,"text/plain","Malformed or oversized setup request.",now);
      else if (request_.complete) handle(now,settings,clock);
      else if (!client_.connected()) closeClient();
    }
  }
  if (!clientActive_ && testAfterResponse_) {
    if(server_.hasClient()) {auto pending=server_.accept();pending.stop(1);}
    testAfterResponse_=false; server_.end();
    startStation(candidate_,State::Testing);
  }
}
