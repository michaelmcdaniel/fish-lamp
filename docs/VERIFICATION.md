# Verification record

## Wi-Fi indicators and ZIP provisioning — 2026-10-27

* Onboard LED uses a nonblocking 250 ms blink phase while joining and stays on
  for a cached station connection. It is off in AP, offline and retry-wait states.
  Real/mock adapter tests cover blink phases, steady connection and disconnect.
* OLED network row cycles every five seconds through cached SSID, station IPv4
  address and RSSI. Tests verify the complete cycle and no display-owned RSSI
  reads. Long names scroll; unavailable RSSI and unsupported font characters are
  explicitly represented. Other rows, including 500 ms ambient updates, remain.
* Wi-Fi setup asks for a five-digit US ZIP; None instead shows required manual
  coordinates and the optional browser-location button. Node tests verify field
  visibility/required/disabled state, leading zeros, invalid ZIP rejection,
  offline/online switching, browser clock/zone and permission failures.
* Compiled GeoNames table contains 41,488 sorted US ZIP records, 497,856 bytes
  of flash. Source, conversion, license and archive checksum are recorded in
  `ZIP-DATA.md`; builds do not download data. Existing coordinate-only configs
  remain valid under schema 1. Optional `zip-code` validation rejects malformed
  codes, and setup saves resolved coordinates plus `location-source` through the
  existing verified config replacement. Tests reload saved coordinates and
  confirm imported lighting remains present.
* Missing ZIPs use one asynchronous Zippopotam.us DNS/TCP/HTTP request, with a
  six-second deadline, 4 KiB response cap and a 2 KiB JSON arena on Pico. Tests
  cover fragmented/chunked replies, real response fields, invalid coordinates,
  oversized replies, DNS/connect failure, timeout through millis wrap, late DNS
  callbacks after cancellation, and the fallback 35.84562, -86.39027. A public
  example request for ZIP 90210 returned HTTP 200 and expected coordinates from
  the development computer; this does not establish on-device API connectivity.
* `./test.ps1` passed **218,268 checks**, real/mock adapter suites and OLED
  enabled/disabled suites. `node tests/test_setup_page.cjs` passed. Physical LED,
  OLED readability, DNS/TCP timing, real router/Internet failure and watchdog
  progress still need measurement on the Pico WH. No Pico 2 W build was run.
* Final `./build-picowh.ps1` passed: **951,628 bytes flash**, **127,252 bytes
  static RAM**, **134,892 bytes remaining**. Only dependency warnings from
  Adafruit NeoPixel, Wi-Fi headers, OneWire and U8g2 appeared; no firmware-source
  warnings or errors were emitted.

## Home Wi-Fi testing transition and readable errors — 2026-10-27

* Installed Arduino-Pico 6.0.0/6.3.1 source inspection found that `WiFi.end()`
  leaves AP mode active; `beginNoBlock()` in that mode starts an AP rather than
  joining the router. Also, `softAPConfig()` before selecting AP mode configures
  the station address instead. The corrected sequence explicitly disconnects
  the AP, selects the destination mode, and resets the station to DHCP.
  Connection success now requires station mode, not an AP's connected status.
* Updated Wi-Fi fakes reproduce these core behaviors. Regression tests failed
  against the old setup sequence, then passed with the fix. Tests cover failed
  joins, AP recovery, station-only success, DHCP reset and distinct radio error
  messages without exposing credentials.
* OLED errors cycle through 21-character numbered pages every three seconds.
  Tests reconstruct the complete error across pages and verify the BOOTSEL
  marker cannot hide a setup failure. USB Serial logs complete setup errors.
* `./test.ps1` passed **218,188 checks**, both real/mock device suites and both
  OLED enabled/disabled suites. `./build-picowh.ps1` passed: 442,900 bytes flash,
  123,036 bytes static RAM, 139,108 bytes remaining. Only existing dependency
  warnings from Adafruit NeoPixel and core Wi-Fi headers were emitted.
* Physical router connection and radio timing still require the owner's Pico WH.
  Compilation and simulated tests do not establish successful hardware joining.
  Only the Pico WH is being compiled for current testing, per owner instruction.

## Setup UX, captive portal, BOOTSEL and ambient display — 2026-10-27

* Replaced the datalist with a native SSID select. Manual reveals an SSID input;
  None selects offline operation and hides credentials. Refresh beside the label
  fetches the pre-AP scan snapshot, with bounded retries and explicit errors.
  `/networks` now includes count and numeric scan-start error; Serial logs those
  fields once without revealing SSIDs. The bounded callback scan was checked
  against the installed core's scan initiation/callback interfaces. No physical
  radio discovery result is claimed.
* Browser defaults use its current UTC clock and detect fixed-offset or US-rule
  zones by comparing reported offsets, including transition instants. Advanced
  overrides expose coordinates, standard offset/rule, and UTC clock override.
  Unsupported seasonal zones require review rather than receiving an invented
  rule. Location uses `enableHighAccuracy:false`, accepts an already available
  position, and has an eight-second timeout. HTTP setup normally cannot obtain
  browser geolocation, so it leaves coordinates blank and exposes Advanced.
* Added core DNSServer wildcard resolution (one packet per loop, 512-byte core
  limit, no forwarding) and captive HTTP redirects to the canonical setup IP.
  Foreign-host GET requests cannot expose session tokens; configuration POST
  Host/Origin/token validation remains intact. DNS stops before station testing.
  Automatic popup is OS-dependent and has not been tested on a real phone.
* BOOTSEL uses the core's flash-safe reader. A debounced pressed state persists
  a tiny verified/renamed request marker through ConfigManager. Release while
  running clears it once; holding through loss of power leaves it for startup
  and forces setup with the saved lighting configuration retained. Successful
  setup saving also clears it. OLED/Serial acknowledge the armed request. Tests
  cover release cancellation, short presses, power-cycle persistence, successful
  save, partial writes, repeated holds and millis wrap. Actual physical button,
  filesystem operation and unplug/release timing remain unverified.
* Ambient sampling and cached OLED snapshots now run on independent **500 ms**
  timers. Tests verify a new ADC sample/display row at 500 ms, no sample at 499 ms,
  and no extra water conversions or CPU samples. Dirty rows remain chunked.
* `./test.ps1` passed **218,155 checks**, real/mock device tests and OLED-enabled/
  disabled tests. `node tests/test_setup_page.cjs` passed dropdown/control-collision,
  refresh/retry, None/Manual payloads, normal-accuracy options, clock override,
  Chicago/Phoenix/Kathmandu/London zone handling and permission/late-location tests.
* Rendered the actual embedded page in the in-app browser against the loopback
  preview API with synthetic data. Confirmed two SSIDs initially, a third after
  Refresh without navigation, hidden credentials for None, and visible SSID input
  for Manual. The preview disables geolocation permission and never writes settings.
* Final plain builds passed with Arduino-Pico **6.3.1** and U8g2 **2.36.19**:
  * Pico WH: **442,020 bytes flash**, **122,932 bytes static RAM**, **139,212 bytes**
    remaining. UF2: `build/picowh/fish-lamp.ino.uf2`.
  * Pico 2 W: **434,472 bytes flash**, **125,372 bytes static RAM**, **398,916 bytes**
    remaining. UF2: `build/pico2w/fish-lamp.ino.uf2`.
  Existing core Wi-Fi, NeoPixel, OneWire and unused U8g2-driver warnings remain.
  No firmware upload, real phone association, GPIO measurement or soak test was
  performed. These results establish software checks, not hardware reliability.

## Open setup AP at owner's request — 2026-10-27

* Changed FishLamp-Setup to the installed core's open-AP call with no password.
  Removed the SetupSecrets header dependency and AP password length assertion.
  Session-token validation and home Wi-Fi credential handling remain in place.
  Existing private access files were not changed and are no longer used.
* `./test.ps1` passed: **218,128 checks**, both device-adapter profiles, and
  OLED-enabled/disabled suites. Setup tests now require a null AP password.
* Plain `./build-picowh.ps1` passed: **428,420 bytes flash**, **122,568 bytes
  static RAM**, **139,576 bytes** remaining for stack/heap. Core Wi-Fi warnings
  remain. Updated UF2: `build/picowh/fish-lamp.ino.uf2`.
* No device flashed or physical Wi-Fi association tested during this change.

## Default build library selection — 2026-10-27

* Reproduced plain `./build-picowh.ps1` failure: the compiler could not read
  U8g2 sources from the shared OneDrive library installation (`Invalid argument`).
* Both board scripts now automatically prefer the existing clean
  `build/dependencies/U8g2-2.36.19` package. An explicit `-U8g2Path` still takes
  priority; without a local package, normal Arduino library discovery applies.
* Plain `./build-picowh.ps1` then passed without extra arguments: **428,444 bytes
  flash**, **122,568 bytes static RAM**, **139,576 bytes** remaining for stack/heap.
  Dependency warnings remain. Pico 2 W script syntax was checked; this build-script
  fix did not change firmware, pins, board selections or flash layouts.

## OLED diagnostics — 2026-10-27

* Confirmed Postal Clock's display class uses SH1106 128×64 with U8g2. Added
  optional `StatusDisplay` with Wire1 GPIO 2/3, leaving RTC Wire GPIO 16/17 alone.
  OLED-disabled compilation removes display I/O and its rendering dependency.
* `./test.ps1` passed: **218,128 checks**, real/mock device-adapter suites, and
  OLED-enabled/disabled suites. Display tests cover startup, fixed 25-character
  rows, local time, stale/unavailable readings, applied brightness range, cached
  row transfers, at most one I2C transaction per tick, RTC bus isolation, missing
  display, mid-transfer failure/no retry, and millis wrap. RSSI tests cover cached
  samples, unavailable values without rapid retries, and disconnect invalidation.
  The fake renderer does not validate physical glyphs or orientation.
* The assertion test initially exposed a display timezone units error (the
  existing `offsetAt()` already returns seconds); corrected it. Also fixed the
  host runner to propagate negative assertion-abort exit codes instead of hiding
  them behind a later successful test.
* Final Arduino-Pico **6.3.1** builds passed with U8g2 **2.36.19** enabled:
  * Pico WH: **428,444 bytes flash**, **122,568 bytes static RAM**, 139,576 bytes
    remaining for stack/heap. UF2: `build/picowh/fish-lamp.ino.uf2`.
  * Pico 2 W: **421,096 bytes flash**, **125,052 bytes static RAM**, 399,236 bytes
    remaining for stack/heap. UF2: `build/pico2w/fish-lamp.ino.uf2`.
* The shared OneDrive U8g2 installation had unreadable cloud files. Used the
  official Arduino U8g2-2.36.19 archive in ignored `build/dependencies/` with the
  scripts' optional `-U8g2Path` argument; the shared installation was preserved.
  Archive SHA-256: `c9de6e5a5aa2e2884d3296c6a86be4bc2770e27b11d4302ab5134273ccb6a670`.
  Build warnings were in NeoPixel, OneWire, core Wi-Fi and an unused U8g2 driver.
* No firmware upload or physical test performed. Display/controller identity,
  actual address, readability, measured I2C/control timing, stuck-bus behavior
  and the offline lighting soak test remain hardware acceptance work.

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

10. OLED commissioning (not yet performed): confirm SH1106 controller and 0x3C
    or 0x3D address, SDA GPIO 2 / SCL GPIO 3 on Wire1, 3.3 V supply/pull-ups,
    startup text and readable eight-row layout. Verify RTC on GPIO 16/17 continues
    working. Compare displayed Fahrenheit, ambient percentage, applied grow state,
    day/night/manual modes and per-pixel brightness range with Serial/state.
    Disable Wi-Fi, unplug/reconnect water sensor and OLED, hold OLED SCL low,
    and confirm bounded display failure with continuing lighting and watchdog
    progress. Display recovery after a bus fault intentionally requires restart.
    Measure actual maximum loop/transfer times and run the 24-hour offline test
    with OLED enabled and disabled; host fakes cannot establish those results.

## Connected dashboard checks

Host coverage: station connect/reconnect, `/` and both status URLs, 85 day/night
arrays, JSON escaping and password exclusion, stale/invalid sensor values,
invalid clock, unsupported methods/routes, oversized headers, request bodies,
stalled readers/writers, and continued scheduling during HTTP faults. Run
`./test.ps1` and `node tests/test_dashboard_page.cjs`; the latter executes the
embedded script against DOM/fetch stand-ins, including stale polling recovery
and future events across both US DST transitions. It is not browser visual QA.

On device: visit the shown station IP on a phone; compare every sensor and both
LED arrays with Serial/configuration, observe an actual day/night/grow boundary,
exercise manual overrides via Serial, then stop Wi-Fi during polling. Confirm
stale UI, continued outputs, reconnect/reload recovery and no watchdog resets.
Test a client that trickles headers and a client that never reads its response;
measure maximum complete loop time (including web work), snapshot latency and
heap trends during prolonged polling. These hardware checks remain unperformed.

Release gate: do not call this a reliable replacement appliance until the
hardware checklist and deferred essential commissioning/services are complete.
