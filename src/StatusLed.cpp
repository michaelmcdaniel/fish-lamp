#include "StatusLed.h"
#include "Devices.h"
#include "MockDebug.h"
#include <Arduino.h>
void StatusLed::begin() {
  if(devices::status) {digitalWrite(devices::statusLedPin,LOW);pinMode(devices::statusLedPin,OUTPUT);}
  else mockdebug::log("mock status LED=0\r\n");
}
void StatusLed::apply(bool on) {
  if(on_==on) return;
  on_=on;
  if(devices::status) digitalWrite(devices::statusLedPin,on ? HIGH : LOW);
  else mockdebug::log("mock status LED=%d\r\n",on);
}
void StatusLed::tick(uint32_t now,bool connecting,bool connected) {
  apply(connected || (connecting && (now/250)%2==0));
}
