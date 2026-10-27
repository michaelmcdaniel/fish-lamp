#include "SetupButton.h"
#include "ConfigManager.h"
#include "MockDebug.h"
#include <Bootsel.h>

void SetupButton::tick(uint32_t now,ConfigManager& settings) {
  if(uint32_t(now-sampledMs_)<PollMs) return;
  sampledMs_=now;
  // Use the core's supported flash-safe reader, not a GPIO assignment.
  if(!BOOTSEL) {
    if(armed_) {
      if(settings.clearSetupRequest()) mockdebug::log("BOOTSEL released: setup request cancelled.\r\n");
      else mockdebug::log("Could not clear setup request; setup remains requested for next boot.\r\n");
      armed_=false;
    }
    pressed_=false;handled_=false;return;
  }
  if(!pressed_) {pressed_=true;pressedMs_=now;}
  if(handled_ || uint32_t(now-pressedMs_)<HoldMs) return;
  handled_=true; // At most one write attempt per hold, including failures.
  armed_=settings.requestSetup();
  if(armed_)
    mockdebug::log("Setup armed while BOOTSEL held. Unplug while holding, then release before reconnecting power.\r\n");
  else mockdebug::log("Could not save setup request. Release BOOTSEL before trying again.\r\n");
}
