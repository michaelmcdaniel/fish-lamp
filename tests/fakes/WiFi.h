#pragma once
#include <stdint.h>
#include <string>
#include <memory>
#include <deque>
#include <algorithm>
#include <cstring>
constexpr int WL_CONNECTED=3;
struct IPAddress { IPAddress(int,int,int,int) {} };
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
  bool apOK=true, protectedAP=false;
  unsigned starts=0, ends=0, stationAttempts=0;
  int statusCode=0;
  void setTimeout(unsigned) {}
  void end() { ++ends; statusCode=0; }
  void softAPConfig(IPAddress,IPAddress,IPAddress) {}
  bool softAP(const char*,const char* password) { ++starts; protectedAP=password && strlen(password)>=8; return apOK; }
  void beginNoBlock(const char*,const char*) { ++stationAttempts; }
  int status() { return statusCode; }
};
inline FakeWiFi WiFi;
