# Verification record — 2026-10-26

## Executed

* Built safe outputs, RTC/time, solar math, integrated configuration/scheduler,
  and diagnostics slices with the existing `build.ps1` using a process-only
  PowerShell execution-policy bypass. Arduino-Pico 6.0.0, Pico 2 W and original
  `flash=4194304_262144` selection retained.
  Final image: **356,204 bytes flash (9%)**, **90,392 bytes static RAM (17%)**,
  with 433,896 bytes reported remaining for heap/stack. This is the compiler's
  allocation report, not a measured runtime high-water mark.
* Final host suite: **218,030 assertions passed** under MSVC C++17, `/W4`, no
  host compiler warnings. The count includes exhaustive dates and repeated
  simulated time steps, not that many independent scenarios.
* Covered all valid dates 2000–2099, leap days, local midnight, US DST spring/fall
  boundaries, fractional timezone offsets, ambiguous/missing local times,
  invalid RTC BCD/date/OSF/century, 12-hour RTC encoding, exact on/off boundaries,
  solar sanity at equinox, polar missing events, offsets/max limits, independent
  grow windows, overnight RGB and monotonic override expiry across millis wrap.
* Simulated 48 hours of scheduling, and separately 48 hours of RTC bus failure
  with advancing holdover, retry limits, recovery and an invalid follow-up read.
  Fake Wire asserts the actual adapter's pin selection and timeout. No network
  implementation is present; this does not test future networking interference.
* Actual output controller exercised with a fake strip/GPIO adapter: correct
  requested pins/count/order, per-pixel scaling, safe initial output, complete
  force-off and no extra redraws or grow writes across 1,000 unchanged updates.
  The fake does not validate PIO waveforms or physical polarity.
* Configuration: strict sizes/types/ranges, missing fields, unconfirmed UTC,
  unsupported schema/DST, oversized input, truncation at every byte, trailing
  objects/garbage, preservation of destination on failure.
* Migration script passed synthetic tests for retained LED/date fields,
  unchanged source, refusal to overwrite, and rejection of invalid channels.
* CLI board discovery reported two unknown serial ports, no identified Pico.
  No firmware upload, filesystem modification or physical experiment performed.

Arduino dependency warnings remain in Adafruit NeoPixel 1.15.2's PIO program
initializer (`pio_version`, `used_gpio_ranges`). The installed SDK accepts the
zero-initialized fields and linking succeeds; on-device behavior remains untested.
No user firmware source warnings were emitted in the final build.

## Hardware acceptance — all pending

Record board/core/library versions, supply, RTC battery status, firmware revision,
test start/end UTC, configured timezone, maximum measured loop time, reset cause,
and observed output transitions. Keep credentials and private settings out of logs.

1. Commission a compatible LittleFS image and DS3231 UTC separately. Validate the
   JSON with `test-build/tests.exe --validate <path>` first. Confirm RTC OSF is
   cleared only after deliberate clock initialization. Back up existing settings
   before any firmware/filesystem replacement.
2. Start with the actual aquarium loads disconnected for GPIO polarity checks.
   Cold boot, reset, missing/corrupt config, failed mount, invalid RTC and absent
   RTC must produce black RGB / grow LOW. Check boot glitches with instruments;
   external pull-downs may be needed before firmware takes control.
3. Verify all 85 pixels, GRB order, per-pixel brightness, day/night transition,
   grow HIGH polarity, and force-off/override expiry with real loads. Confirm a
   black day array stays black. Compare solar boundaries to independent events
   for the configured location and date.
4. Observe at least 24 hours with Wi-Fi absent and Serial disconnected. Cross
   midnight and verify both schedules. Record RTC drift, control-loop maxima and
   every unexpected reset. The desktop simulation is not this soak test.
5. Disconnect RTC after a validated read; verify holdover and stale indication.
   Test stuck I2C lines and reconnect; measure actual transaction/control times.
   Correct the clock forward/back across a local date boundary; verify schedule.
6. Exercise missing/invalid primary and valid backup. With no valid file, require
   safe-off. Confirm no auto-format and no routine flash writes. Atomic-save
   power-cut testing remains blocked on the future save implementation.
7. Test watchdog recovery with a deliberately stalled test build, restore the
   normal build, and verify safe boot and reported reset reason. Check no rapid
   reboot cycle under normal RTC faults or disconnected USB.
8. Record memory/heap trends and actual strip frame time. A nominal 85-pixel
   800 kHz frame takes roughly 2.55 ms plus latch/driver overhead; this is an
   estimate, **not a measured result**. Measure USB backpressure behavior too.
9. After sensors/networking are implemented, perform the remaining AGENTS.md
   DS18B20, DNS/NTP, hostile/slow HTTP client, telemetry outage, reconnection,
   protected AP and API/UI compatibility tests. None has passed yet.

Release gate: do not call this a reliable replacement appliance until the
hardware checklist and deferred essential commissioning/services are complete.
