#include "StatusLed.h"
#include "Devices.h"
#include "MockDebug.h"
#include <Arduino.h>
void StatusLed::begin() {
  if(devices::status) {digitalWrite(LED_BUILTIN,LOW);pinMode(LED_BUILTIN,OUTPUT);}
  else mockdebug::log("mock status LED=0\r\n");
}
void StatusLed::apply(bool on) {
  if(on_==on) return;
  on_=on;
  if(devices::status) digitalWrite(LED_BUILTIN,on ? HIGH : LOW);
  else mockdebug::log("mock status LED=%d\r\n",on);
}
