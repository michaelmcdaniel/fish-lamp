#include "SetupService.h"
#include <LittleFS.h>
#include <Wire.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <iostream>
#include <cstdlib>

static unsigned assertions=0;
#define REQUIRE(x) do{++assertions;if(!(x)){std::cerr<<"FAIL setup line "<<__LINE__<<": " #x "\n";std::exit(1);}}while(0)
static std::string candidate(const std::string& fixture,bool wifi=false) {
  JsonDocument d; deserializeJson(d,fixture);
  d["setup-utc"]=clockmath::midnight({2026,10,26});
  if(wifi) { d["wifi-sid"]="synthetic-network"; d["wifi-pwd"]=""; }
  std::string result; serializeJson(d,result); return result;
}
static std::string get(const char* path) { return std::string("GET ")+path+" HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n"; }
static std::string post(const std::string& body,const std::string& token) {
  return "POST /configure HTTP/1.1\r\nHost: 192.168.4.1\r\nContent-Type: application/json\r\nX-Setup-Token: "+token+"\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;
}
static void feed(SetupRequest& r,const std::string& data) { for(char ch:data)r.feed(ch); }
static void resetHardware() { LittleFS=FakeFS{};Wire=FakeWire{};WiFi=FakeWiFi{};rp2040=FakeRp2040{};fakeScan=FakeScan{};fakeIncoming.clear();fakeMillis=0; }
static void scanResult(const std::string& name,int rssi=-60) {
  cyw43_ev_scan_result_t result;
  result.rssi=int16_t(rssi);result.ssid_len=uint8_t(name.size());
  memcpy(result.ssid,name.data(),std::min(name.size(),sizeof(result.ssid)));
  fakeScan.callback(fakeScan.context,&result);
}
static void pump(SetupService& s,ConfigManager& c,TimeService& t,unsigned ticks=1) {
  for(unsigned i=0;i<ticks;++i) { ++fakeMillis;s.tick(fakeMillis,c,t); }
}
static std::shared_ptr<FakeSocket> connect(const std::string& input) {
  auto socket=std::make_shared<FakeSocket>();socket->input=input;fakeIncoming.push_back(socket);return socket;
}
static std::string exchange(SetupService& s,ConfigManager& c,TimeService& t,const std::string& input) {
  auto socket=connect(input);
  for(unsigned i=0;i<2000 && socket->live;++i)pump(s,c,t);
  REQUIRE(!socket->live && socket->stopWait==1);
  return socket->output;
}
static std::string token(SetupService& s,ConfigManager& c,TimeService& t) {
  auto response=exchange(s,c,t,get("/session"));
  auto start=response.find("\r\n\r\n");REQUIRE(start!=std::string::npos);
  JsonDocument d; REQUIRE(!deserializeJson(d,response.substr(start+4)));
  auto value=d["token"].as<std::string>(); REQUIRE(value.size()==32);return value;
}
unsigned testSetup(const std::string& fixture) {
  const std::string body=candidate(fixture), networkBody=candidate(fixture,true);
  Config parsed;int64_t utc=0;
  REQUIRE(!parseConfig(fixture.data(),fixture.size(),parsed,&utc));
  REQUIRE(parseConfig(body.data(),body.size(),parsed,&utc));
  REQUIRE(utc==clockmath::midnight({2026,10,26}));
  SetupRequest req;
  feed(req,get("/")); REQUIRE(req.complete && !req.error && req.route==SetupRequest::Route::Page);
  req.reset();feed(req,post(body,std::string(32,'0'))); REQUIRE(req.complete && !req.error && req.bodyLength==body.size());
  for(const std::string& headers : {
      "Content-Length: 1\r\nContent-Length: 1\r\n",
      "Content-Length: -1\r\n", "Content-Length: 999999999999999999\r\n",
      "Transfer-Encoding: chunked\r\n", "Origin: http://evil.invalid\r\n"}) {
    req.reset();feed(req,"POST /configure HTTP/1.1\r\nHost: 192.168.4.1\r\n"+headers+"\r\n"); REQUIRE(req.error!=0);
  }
  req.reset();feed(req,"GET / HTTP/1.1\r\nHost: evil.invalid\r\n\r\n");REQUIRE(req.error==400);
  req.reset();feed(req,std::string(2050,'a'));REQUIRE(req.error==431);
  req.reset();feed(req,post(body,std::string(31,'0')));REQUIRE(req.error==403);
  req.reset();feed(req,get("/session").substr(0,10));REQUIRE(!req.complete && !req.error);
  req.reset();feed(req,get("/networks"));REQUIRE(req.complete && req.route==SetupRequest::Route::Networks);

  resetHardware();NetworkScan scan;fakeScan.active=true;scan.begin(0xfffffff0u);
  scanResult("synthetic-one",-70);scanResult("synthetic-one",-30);
  scanResult("synthetic-two",-50);scanResult("quote\"slash\\line\n",-60);
  scanResult("caf\xc3\xa9",-80);scanResult("");scanResult(std::string("bad\0name",8));
  scanResult(std::string("\xc0\xaf",2));scanResult(std::string(32,'x'),-90);
  REQUIRE(!scan.tick(uint32_t(0xfffffff0u+7999u)));
  REQUIRE(scan.tick(uint32_t(0xfffffff0u+8000u)) && !fakeScan.station);
  JsonDocument scanJson;REQUIRE(!deserializeJson(scanJson,scan.json()));
  REQUIRE(scanJson["status"]=="timeout" && scanJson["networks"].size()==5);
  REQUIRE(scanJson["networks"][0]=="synthetic-one");
  REQUIRE(scanJson["networks"][2]=="quote\"slash\\line\n");
  REQUIRE(scanJson["networks"][3]=="caf\xc3\xa9");
  const std::string snapshot=scan.json();scanResult("late-callback",-10);REQUIRE(snapshot==scan.json());
  fakeScan=FakeScan{};scan.begin(0);
  for(int i=0;i<60;++i)scanResult("synthetic-"+std::to_string(i),-100+i);
  REQUIRE(scan.tick(1));REQUIRE(!deserializeJson(scanJson,scan.json()));
  REQUIRE(scanJson["status"]=="ready" && scanJson["networks"].size()==24);
  REQUIRE(scanJson["networks"][0]=="synthetic-59");
  fakeScan=FakeScan{};fakeScan.error=-1;scan.begin(0);REQUIRE(scan.tick(1));
  REQUIRE(!deserializeJson(scanJson,scan.json()) && scanJson["status"]=="failed");

  resetHardware();ConfigManager discovery;discovery.begin();TimeService discoveryClock;discoveryClock.begin();SetupService discovering;
  fakeScan.active=true;discovering.begin(discovery);scanResult("synthetic-nearby");
  pump(discovering,discovery,discoveryClock,7999);REQUIRE(WiFi.starts==0 && discovering.state()==SetupService::State::Scanning);
  pump(discovering,discovery,discoveryClock);REQUIRE(WiFi.starts==1 && !fakeScan.station);
  auto listed=exchange(discovering,discovery,discoveryClock,get("/networks"));
  REQUIRE(listed.find("synthetic-nearby")!=std::string::npos && listed.find("timeout")!=std::string::npos);
  REQUIRE(fakeScan.starts==1); // Requests serve a snapshot, never trigger scanning.
  auto first=connect(get("/session"));pump(discovering,discovery,discoveryClock,4);
  auto queued=connect(get("/networks"));pump(discovering,discovery,discoveryClock,600);
  REQUIRE(!first->live && !queued->live && queued->output.find("200 OK")!=std::string::npos);

  resetHardware();LittleFS.mountOK=false;
  ConfigManager storage;storage.begin(); REQUIRE(!storage.valid() && LittleFS.formats==0 && !LittleFS.autoFormat);
  REQUIRE(storage.save(body.data(),body.size()));REQUIRE(LittleFS.formats==1 && storage.valid());
  LittleFS.shortWrite=true; REQUIRE(!storage.save(body.data(),body.size()));
  ConfigManager old;old.begin();REQUIRE(old.valid());
  LittleFS.shortWrite=false;LittleFS.renames=0;LittleFS.failRenameAt=1;
  REQUIRE(!storage.save(body.data(),body.size()));old.begin();REQUIRE(old.state==ConfigState::Primary);
  LittleFS.renames=0;LittleFS.failRenameAt=2;
  REQUIRE(!storage.save(body.data(),body.size()));
  ConfigManager recovery;recovery.begin();REQUIRE(recovery.state==ConfigState::Backup);
  // A valid temporary file alone is not an acknowledged configuration.
  LittleFS.files.erase("/config.bak");ConfigManager incomplete;incomplete.begin();REQUIRE(!incomplete.valid());
  resetHardware();TimeService rtc;rtc.begin();Wire.data[15]=0x83;
  REQUIRE(rtc.setUtc(clockmath::midnight({2024,2,29})+86399,10));
  REQUIRE(rtc.valid() && Wire.data[15]==3 && rtc.utc()==clockmath::midnight({2024,3,1})-1);
  REQUIRE(!rtc.setUtc(0,20));Wire.failure=true;REQUIRE(!rtc.setUtc(utc,30));

  resetHardware();ConfigManager c;c.begin();TimeService t;t.begin();SetupService s;s.begin(c);pump(s,c,t);
  REQUIRE(s.state()==SetupService::State::AccessPoint && WiFi.protectedAP);
  auto sessionToken=token(s,c,t);
  auto page=exchange(s,c,t,get("/"));REQUIRE(page.find("Set up your Fish Lamp")!=std::string::npos);
  auto denied=exchange(s,c,t,post(body,std::string(32,'f')));REQUIRE(denied.find("403")!=std::string::npos && !c.valid());
  Wire.failure=true;
  auto badRtc=exchange(s,c,t,post(body,sessionToken));REQUIRE(badRtc.find("503")!=std::string::npos && !c.valid() && rp2040.reboots==0);
  Wire.failure=false;LittleFS.shortWrite=true;
  auto badSave=exchange(s,c,t,post(body,sessionToken));REQUIRE(badSave.find("503")!=std::string::npos && !c.valid());
  LittleFS.shortWrite=false;
  auto saved=exchange(s,c,t,post(body,sessionToken));
  REQUIRE(saved.find("200 OK")!=std::string::npos && saved.find("Settings saved")!=std::string::npos);
  REQUIRE(c.valid() && s.restarting() && rp2040.reboots==0);
  pump(s,c,t,4000);REQUIRE(rp2040.reboots>0);
  ConfigManager boot;boot.begin();SetupService afterRestart;afterRestart.begin(boot);
  REQUIRE(boot.valid() && afterRestart.state()==SetupService::State::Disabled);

  resetHardware();ConfigManager nc;nc.begin();TimeService nt;nt.begin();SetupService ns;ns.begin(nc);pump(ns,nc,nt);
  sessionToken=token(ns,nc,nt);
  auto accepted=exchange(ns,nc,nt,post(networkBody,sessionToken));
  REQUIRE(accepted.find("202 Accepted")!=std::string::npos && ns.state()==SetupService::State::Testing && !nc.valid());
  pump(ns,nc,nt,20001);REQUIRE(ns.state()==SetupService::State::AccessPoint && !nc.valid() && rp2040.reboots==0);
  auto error=exchange(ns,nc,nt,get("/session"));REQUIRE(error.find("Could not connect")!=std::string::npos);
  sessionToken=token(ns,nc,nt);
  exchange(ns,nc,nt,post(networkBody,sessionToken));
  WiFi.statusCode=WL_CONNECTED;pump(ns,nc,nt);
  REQUIRE(nc.valid() && ns.restarting());
  SetupService station;station.begin(nc);REQUIRE(station.state()==SetupService::State::StationConnecting);
  pump(station,nc,nt);REQUIRE(station.state()==SetupService::State::StationOnline);
  WiFi.statusCode=0;pump(station,nc,nt);REQUIRE(station.state()==SetupService::State::StationRetry && nc.valid());
  pump(station,nc,nt,60000);REQUIRE(station.state()==SetupService::State::StationConnecting);

  resetHardware();ConfigManager sc;sc.begin();TimeService st;st.begin();SetupService slow;slow.begin(sc);pump(slow,sc,st);
  auto stalled=connect("GET /");pump(slow,sc,st,2002);REQUIRE(!stalled->live && !sc.valid());
  auto blocked=connect(get("/"));blocked->room=0;pump(slow,sc,st,2002);REQUIRE(!blocked->live && !sc.valid());
  auto trickle=connect("G");pump(slow,sc,st);
  for(unsigned i=0;i<8;++i) {pump(slow,sc,st,1900);trickle->input+='E';}
  REQUIRE(!trickle->live && !sc.valid());
  resetHardware();ConfigManager ac;ac.begin();TimeService at;at.begin();SetupService ap;WiFi.apOK=false;ap.begin(ac);pump(ap,ac,at);
  REQUIRE(ap.state()==SetupService::State::APFailed && WiFi.starts==1 && rp2040.reboots==0);
  pump(ap,ac,at,59000);REQUIRE(WiFi.starts==1);
  WiFi.apOK=true;pump(ap,ac,at,1002);REQUIRE(ap.state()==SetupService::State::AccessPoint && WiFi.starts==2);
  return assertions;
}
