#pragma once
#include <stdint.h>
#include <string>
#include <memory>
#include <deque>
#include <algorithm>
#include <cstring>
constexpr int WL_CONNECTED=3;
enum WiFiMode_t { WIFI_OFF=0, WIFI_STA=1, WIFI_AP=2, WIFI_AP_STA=3 };
struct IPAddress {
  int bytes[4];
  IPAddress(int a,int b,int c,int d):bytes{a,b,c,d} {}
  bool isSet() const { return bytes[0] || bytes[1] || bytes[2] || bytes[3]; }
};
struct FakeSocket { std::string input,output; size_t read=0; bool live=true; int room=4096; unsigned stopWait=0; };
struct WiFiClient {
  std::shared_ptr<FakeSocket> socket;
  WiFiClient()=default;
  explicit WiFiClient(std::shared_ptr<FakeSocket> s):socket(s){}
  explicit operator bool() const { return socket && socket->live; }
  void stop(unsigned wait) { if(socket){socket->live=false;socket->stopWait=wait;} }
  void setTimeout(unsigned) {}
  void setNoDelay(bool) {}
  int available() { return socket ? int(socket->input.size()-socket->read) : 0; }
  int read() { return available() ? static_cast<unsigned char>(socket->input[socket->read++]) : -1; }
  bool connected() { return socket && socket->live; }
  int availableForWrite() { return socket && socket->live ? socket->room : 0; }
  size_t write(const uint8_t* data,size_t length) {
    if (!socket || !socket->live) return 0;
    size_t n=std::min(length,size_t(socket->room));
    socket->output.append(reinterpret_cast<const char*>(data),n); return n;
  }
};
inline std::deque<std::shared_ptr<FakeSocket>> fakeIncoming;
struct WiFiServer {
  bool listening=false;
  explicit WiFiServer(int) {}
  void begin(int,int) { listening=true; }
  void end() { listening=false; }
  explicit operator bool() const { return listening; }
  bool hasClient() { return listening && !fakeIncoming.empty(); }
  WiFiClient accept() {
    if(!hasClient())return {};
    auto socket=fakeIncoming.front(); fakeIncoming.pop_front(); return WiFiClient(socket);
  }
};
struct FakeWiFi {
  bool apOK=true, protectedAP=false, openAP=false;
  bool apRunning=false, stationStaticIP=false, apConfigStored=false;
  WiFiMode_t modeValue=WIFI_OFF;
  unsigned starts=0, ends=0, stationAttempts=0;
  unsigned apDisconnects=0, misroutedBegins=0, dhcpResets=0;
  int statusCode=0;
  int32_t signal=-55;
  unsigned signalReads=0;
  void setTimeout(unsigned) {}
  void mode(WiFiMode_t value) { modeValue=value; }
  WiFiMode_t getMode() { return modeValue; }
  // Match installed Arduino-Pico: end/disconnect leave an AP active.
  void end() { ++ends; statusCode=0;if(modeValue==WIFI_STA)modeValue=WIFI_OFF; }
  bool disconnectAP() {
    ++apDisconnects;bool active=apRunning;apRunning=false;
    if(modeValue==WIFI_AP)modeValue=WIFI_OFF;
    return active;
  }
  void softAPConfig(IPAddress ip,IPAddress,IPAddress) {
    if(modeValue==WIFI_AP || modeValue==WIFI_AP_STA)apConfigStored=true;
    else stationStaticIP=ip.isSet();
  }
  void config(IPAddress ip,IPAddress,IPAddress,IPAddress) {
    stationStaticIP=ip.isSet();if(!stationStaticIP)++dhcpResets;
  }
  bool softAP(const char*,const char* password=nullptr) {
    ++starts; openAP=password==nullptr;
    protectedAP=password && strlen(password)>=8;
    apRunning=apOK;modeValue=apOK?WIFI_AP:WIFI_OFF;return apOK;
  }
  int beginNoBlock(const char* ssid,const char* password) {
    if(modeValue==WIFI_AP) {++misroutedBegins;return softAP(ssid,password)?WL_CONNECTED:0;}
    ++stationAttempts;modeValue=WIFI_STA;statusCode=0;return statusCode;
  }
  int status() { return modeValue==WIFI_AP && apRunning ? WL_CONNECTED : statusCode; }
  int32_t RSSI() { ++signalReads; return signal; }
};
inline FakeWiFi WiFi;
