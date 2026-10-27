# Verification record

## New Windows computer toolchain — 2026-10-27

* Installed Arduino CLI 1.5.1 in the user-local Programs directory after verifying
  the official release archive's SHA-256 checksum. Installed Arduino-Pico 6.0.0;
  confirmed ArduinoJson 7.4.3, Adafruit NeoPixel 1.15.2 and OneWire 2.3.8.
* Added `setup-tools.ps1` and CLI discovery shared by build/test scripts. Retained
  the Pico 2 W board and 4 MiB flash / 256 KiB filesystem selection. Test library
  discovery now uses Arduino CLI's resolved sketchbook instead of a fixed path.
* Setup rerun under Windows PowerShell reused the installed dependencies.
  Host tests passed under PowerShell 7 and Windows PowerShell: **218,122 checks**
  plus both real/mock device-adapter suites. Configuration migration checks passed.
* `./build.ps1` passed with the default real-device profile: **409,952 bytes flash**
  and **123,528 bytes static RAM**, leaving 400,760 bytes for stack/heap. Warnings
  were in the NeoPixel, OneWire and core Wi-Fi dependencies; no firmware source
  changes were required.
* Existing private setup access files and user settings were preserved. No firmware
  uploaded and no physical hardware tests performed.

## Device switches and mock adapters — executed

* Default real-device build passed with unchanged build.ps1/FQBN: **409,952 bytes
  flash**, **123,528 bytes static RAM**, 400,760 bytes remaining. OneWire 2.3.8
  installed through Arduino Library Manager for the real DS18B20 adapter.
* All-mock firmware compiled with BENCH=1, CPU_TEMP=0, STATUS_LED=0, WIFI=0:
  **369,344 bytes flash**, **122,452 bytes static RAM**. Same board/flash layout;
  temporary compiler flags and output under ignored test-build/mock-firmware.
* Usable bench profile (BENCH=1, Wi-Fi/internal CPU/status LED real) also passed:
  **406,216 bytes flash**, **123,464 bytes static RAM**. Deployable artifact:
  `test-build/bench-firmware/fish-lamp.ino.uf2`. The normal `build` output remains
  the real-device profile; source defaults remain real hardware.
* **218,122 existing host assertions passed**, plus dedicated real and mock
  device-adapter executables. No host compiler warnings. Tests exercise no GPIO,
  OneWire or I2C operations for disabled devices; fixed mock values; RTC writes
  and progression in RAM; applied LED brightness; repeated-output suppression;
  bounded debug queue overflow; DS18B20 absent/reconnected, CRC failures,
  parasite-power rejection, incomplete conversions and millis wrap; invalid ADC
  and CPU readings retaining last-valid snapshots.
* Firmware warnings remain in existing NeoPixel and core WiFi dependencies.
  No firmware-source warnings. No firmware uploaded or physical tests performed.
* Hardware follow-up: boot bench profile on a bare Pico 2 W, provision via AP,
  inspect mock Serial messages, then enable each attached gadget individually.
  Measure OneWire/ADC execution durations and watchdog margin; verify sensor
  pull-up/external power, temperatures, GPIO polarity and onboard LED indication.

## Setup network combo increment — executed

* Unchanged build.ps1 passed with Arduino-Pico 6.0.0: **407,616 bytes flash
  (10%)**, **118,684 bytes static RAM (22%)**, 405,604 bytes remaining.
  Existing dependency warnings remain; no firmware-source warnings.
* **218,122 host assertions passed**. Added scan tests cover duplicate SSIDs,
  strongest-first ordering, 24-result capacity, full-length names, JSON escaping,
  UTF-8 validation, hidden/binary names, failure, timeout across millis wrap,
  ignored late callbacks, and AP startup only after scanning ends.
* Cached `/networks` response and queued HTTP requests tested. One connection
  can wait in the listen backlog while the preceding response closes.
* Local browser preview verified the combo's three synthetic options (including
  quotes and ampersand), submission of a listed name, manual hidden-name entry,
  and no console errors. No real credentials or live device were used.
  Screenshot: ignored `test-build/network-combo-preview.png`.
* Physical discovery, mobile-browser dropdown behavior, scan durations and
  AP transition under real radio conditions remain hardware acceptance items.

## Setup AP increment — executed

* Arduino-Pico 6.0.0 build passed using unchanged build.ps1 and FQBN.
  Image: **404,352 bytes flash (10%)**, **112,964 bytes static RAM (21%)**,
  411,324 bytes reported remaining for heap/stack. Runtime memory not measured.
* **218,101 host assertions passed**, with no host compiler warnings. Setup
  tests exercise production HTTP parsing, service state transitions, storage and
  RTC adapters against deterministic fake Wi-Fi/Wire/LittleFS transports.
* Tested: protected AP selection; locally served page/session; token rejection;
  malformed, duplicate-length, chunked, oversized and foreign-host/origin requests;
  partial/slow requests and write backpressure; AP retry backoff; candidate Wi-Fi
  failure returning to setup; successful candidate connection before saving;
  offline submit; verified RTC initialization/OSF handling; no reboot on errors;
  saved config loading after restart without another setup AP; normal station
  disconnect/retry while retaining settings.
* Storage tests: no boot formatting; explicit-submit formatting; short writes;
  failures at either rename; old primary/backup recovery; uncommitted temp ignored.
  These model operation failures, not real flash corruption or physical power cuts.
* Migration regression tests passed. Real credentials were not used in fixtures.
* Mobile page verified at a 390-pixel requested viewport in the in-app browser:
  offline form submission, clock confirmation, success-message display, no
  horizontal overflow, and no console errors. A preview server captured synthetic
  JSON; the production C++ parser accepted its 85-pixel arrays and schedule units.
  This did not contact a Pico. Screenshot: ignored `test-build/setup-preview.png`.
* AP password generated into ignored local access files, never printed or staged.
* Compiler warnings are from the existing NeoPixel PIO initializer and the core's
  WiFi/BearSSL overloaded virtual declarations; no firmware-source warnings.
* No physical AP, RTC, flash power-cut, restart, or 24-hour hardware test performed.

## Initial offline slice — historical results

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

1. On blank/invalid configuration, join the protected FishLamp-Setup AP and visit
   http://192.168.4.1/. Exercise home-Wi-Fi and offline branches, wrong credentials,
   disconnected RTC, storage errors, and successful save/restart. Confirm the
   next boot loads settings and does not restart setup. Confirm RTC UTC/readback.
   Back up existing settings before replacing an existing installation.
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
   safe-off and setup AP. Confirm no boot auto-format and no routine flash writes.
   Interrupt power during temp write, flush, verification, backup rename and
   primary rename. Each boot must either load a valid primary/backup or offer
   setup safely; it must never load an uncommitted temp file.
7. Test watchdog recovery with a deliberately stalled test build, restore the
   normal build, and verify safe boot and reported reset reason. Check no rapid
   reboot cycle under normal RTC faults or disconnected USB.
8. Record memory/heap trends and actual strip frame time. A nominal 85-pixel
   800 kHz frame takes roughly 2.55 ms plus latch/driver overhead; this is an
   estimate, **not a measured result**. Measure USB backpressure behavior too.
9. Physically test slow/hostile setup clients, AP/station transitions, repeated
   failed submissions, low-level Wi-Fi initialization times, response delivery
   before disconnect/reboot, and reconnection. After remaining services are
   implemented, test DS18B20, DNS/NTP, telemetry and the legacy API/UI. No hardware
   result for any of these has been recorded yet.

Release gate: do not call this a reliable replacement appliance until the
hardware checklist and deferred essential commissioning/services are complete.
