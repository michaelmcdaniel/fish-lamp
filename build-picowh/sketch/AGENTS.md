#line 1 "C:\\Users\\mmcda\\OneDrive\\Code\\pico\\fish-lamp\\AGENTS.md"
# AGENTS.md — Fish Lamp firmware

## Mission

Convert the existing **Fish Lamp** MicroPython firmware into a **reliable, maintainable C++ appliance** for a **Raspberry Pi Pico 2 W (RP2350)** using the **Arduino-Pico core and Arduino CLI**. Reuse the *reliability principles* demonstrated by the owner's working Postal Clock project: an offline-first essential function, short and bounded operations, explicit state ownership, watchdog-aware progress, cached snapshots, and resilient network recovery.

**Primary success criterion: the aquarium lighting and grow-light schedules continue to operate predictably without Wi-Fi, HTTP clients, an NTP server, or a functioning temperature-reporting endpoint.** Code that merely compiles but has unreliable behavior is not finished.

The existing MicroPython application is the **behavioral reference**, not an architecture to copy line-for-line. Preserve intended functionality and compatibility while correcting demonstrated defects. Do not assume the prior generated C++ scaffolding is correct.

## Repository and build — preserve these

Reference archive layout:

```text
fish-lamp/
  fish-lamp.ino              # Arduino entry point (currently minimal)
  build.ps1                  # Existing arduino-cli build
  build/                     # Generated files — do not hand-edit
  python/
    main.py                  # Primary active MicroPython behavior
    settings.py              # JSON load/save
    suncalc.py               # Sunrise/sunset calculations
    rtc_lib.py               # DS3231 reference driver
    ds18x20.py, onewire.py   # Temperature sensor drivers
    neopixel.py              # RGB strip reference driver
    datetime.py              # Custom time utilities
    schedule.py              # Incomplete/invalid; NOT imported by main.py
    config.json              # Reference schema; treat values as private
```

The repository may also contain newer C++ files created since this archive; **inspect the current tree before editing**, if present.  do not preserve Defects.

The existing build command in `build.ps1` uses:

```powershell
arduino-cli compile --fqbn 'rp2040:rp2040:rpipico2w:flash=4194304_262144' --warnings all --build-path build .
```

- **Do not replace** Arduino-Pico with ESP32, PlatformIO, standalone Pico SDK/CMake, or FreeRTOS without explicit authorization.
- Preserve `build.ps1` and the board/flash selection unless a verified requirement demands a specific change. The selected flash layout reserves a small filesystem partition; account for its capacity.
- Keep `fish-lamp.ino` small: initialize hardware/services, call their periodic `tick()`/`update()` methods, service the watchdog, and avoid embedding business logic or full HTML in it.
- Use C++ appropriate for embedded systems. Avoid gratuitous singletons, frameworks, threads, overabstracted interfaces, and unbounded dynamic allocations.
- Never say a build or hardware test passed unless it was actually run and its result checked.

## Actual Fish Lamp hardware — do not change pins casually

| Device/function | Pin or detail | Notes |
|---|---|---|
| RGB addressable LED strip | **GPIO 7** | **85 LEDs**, **GRB** physical color ordering; software expresses red/green/blue plus brightness |
| Grow-light switching output | **GPIO 8** | Existing Python uses **HIGH = on, LOW = off**; confirm electrical polarity on device before changing behavior |
| DS18B20 water temperature | **GPIO 26** | OneWire bus; sensor read/convert cycle must not block controller |
| Ambient photoresistor | **GPIO 27 / ADC** | Python normalizes 16-bit ADC reading to approximately 0–100% |
| DS3231 RTC SDA | **GPIO 16** | I2C, RTC reference driver `python/rtc_lib.py` |
| DS3231 RTC SCL | **GPIO 17** | I2C |
| Onboard CPU temperature | **internal ADC channel 4** | **Not GPIO 4**; report separately from water temperature |
| Onboard status LED | **`LED`** | On Pico 2 W this may be controlled through Wi-Fi chip; use supported board API |
| Controller | **Raspberry Pi Pico 2 W** | RP2350 + CYW43 2.4 GHz Wi-Fi |

LEDS have an external 5V powersource with plenty of amps.

## Existing behavior that the port must retain

### Lighting and scheduling

- Two **85-element** arrays: `lights` (day) and `nightlights` (night). Each element uses **`[red, green, blue, brightness]`**, components in 0–255. Physical strip outputs in **GRB** order.
- Sunrise and sunset scheduling from `python/suncalc.py`; `python/main.py` owns active `getSchedule()` and `lighting()` logic.
- Independent grow-light schedule and RGB lighting schedule, each with configurable offsets and absolute-off limits.
- Preserve configuration keys and units for:
  - `lights-sunrise-offset`, `lights-sunset-offset`, `lights-max-offset`
  - `growlight-sunrise-offset`, `growlight-sunset-offset`, `growlight-max-offset`
- The old manual override defaults are **3,600 seconds (one hour)** for both RGB lights and grow light; preserve intended overrides and restoration at the next applicable schedule boundary.
- Separate **desired state** (schedule / manual override) from **applied hardware state**. Apply hardware changes only when necessary; never write flash for a routine scheduler tick.
- Preserve `lightsForceOff` semantics as an intentional override, but do not reproduce accidental race conditions or incorrect priority order.
- Do not blindly translate the existing sunrise/DST logic: its hand-written conversions and `schedule.py` contain defects. Build testable, deterministic UTC/local conversions and scheduling logic.

### Time: borrow the Postal Clock's offline-first design

- The Postal Clock successfully uses its **DS3231 as the authoritative clock while offline** and network time as an optional synchronization source. Apply this *design principle* here, while checking the Fish Lamp's actual connected DS3231.
- At boot, validate RTC availability and plausible date/time; when valid, use it for schedules even without Wi-Fi. NTP is a bounded, opportunistic synchronization source, not a prerequisite to lighting.
- Keep the RTC and software timebase in a consistent convention (prefer UTC internally); do timezone/DST conversions at well-defined boundaries. Avoid DST adjustments twice.
- The reference `suncalc.py` has built-in geographic defaults, not necessarily current runtime configuration. Do not hardcode location or timezone from assumptions; implement explicit configured coordinates and timezone when provisioning is added, with documented migration/defaults.
- Recalculate daily solar events at the correct local date boundary and after a significant clock correction/config change; handle missing time, DST changes, midnight transitions, and first boot deterministically.
- Never let a failed DNS/NTP call stall the lighting scheduler.

### Sensors and telemetry

- DS18B20 water-temperature readings were sampled approximately **once per minute**; conversion time in Python is around **750 ms**. Trigger conversion, return to the main loop, and read when ready rather than `delay(750)` in the control loop.
- The internal Pico temperature is sampled and averaged separately; ambient ADC is exposed to the UI/status output.
- The old code reports water temperature in **°F**. Keep unit semantics explicit; validate disconnected/implausible sensor values without turning them into legitimate readings.
- The configuration key `temperature-url` enables periodic HTTP POST telemetry. Preserve the expected `temperature` property and document compatibility fields; the old code sent fixed humidity/pressure placeholder values, **not measured sensor data**.
- Failed sensors, DNS, HTTP, or telemetry endpoints must not reset or prevent lighting. Rate-limit retries and logs; close network clients reliably.

### Existing HTTP/UI contract

The active MicroPython web server listens on **port 80**, exposes a Vue-based browser UI, and includes these routes or route families. Preserve URL and JSON behavior wherever reasonably possible; document intentional compatibility changes:

```text
GET /
GET /status
GET /lights/on
GET /lights/off
GET /lights/brightness/{value}
GET /lights/{index}/{hexcolor}
GET /lights/{start}-{end}/{hexcolor}
GET /lights/{hexcolor}
GET /grow-light/on
GET /grow-light/off
GET /grow-light
GET /log/temp           and /log/temp/on  and /log/temp/off
GET /log/schedule       and /log/schedule/on and /log/schedule/off
```

- `GET /status` must preserve fields including `lightsOn`, `growlightOn`, `lights`, `schedule`, `growlightDelay`, `lightsDelay`, `waterTemp`, and `ambient` (unless the UI is explicitly migrated).
- The old browser page references external Vue, jscolor, and other JS/CSS resources. Preserve existing UX initially; do not claim it works without Internet unless assets are bundled locally and tested.
- Bound request line/header/body lengths, client timeouts, and concurrent clients. Keep all HTTP interaction separate from real-time light control. Do not allow clients to stall the application.
- Prefer configuration/state APIs over rebuilding huge HTML strings on every request. Avoid excessive heap fragmentation.

### Persistent configuration

- Use the existing LittleFS/JSON plan, verifying available APIs in the installed Arduino-Pico core.
- Keep original `config.json` key spellings where possible; besides the offsets, known keys include `lights`, `nightlights`, `temperature-url`, `wifi-sid`, `wifi-pwd`, `createDate`, and `modDate`.
- Treat network credentials, endpoint URLs, and configuration values as private. **Never log credentials, commit actual passwords, quote them in chat, or embed them in test fixtures.** The supplied Python reference had its Wi-Fi password redacted; do not infer a real password from placeholder text.
- Validate 85 LED entries and 0–255 channels/brightness, ranges for offsets, proper types, and any newly added location/timezone fields.
- Use safe save/replace semantics and write only on actual settings changes. Keep last-known-good configuration on failed writes or interrupted power.
- Do not silently change settings schemas without a migration strategy and tests.

## Reliability principles adapted from Postal Clock

1. **Primary function survives network failure.** Postal Clock keeps displaying time when weather/network is unavailable. Fish Lamp must keep its scheduled light control active when Wi-Fi, NTP, the web server, or telemetry are unavailable.
2. **Cooperative, bounded work first.** Use `millis()` and short state-machine steps instead of `delay()`, retry-forever loops, blocking DNS, long HTTP waits, or networking from within scheduler callbacks.
3. **Use a real hardware watchdog thoughtfully.** Postal Clock used an approximately **8-second watchdog**; start with a documented policy that feeds only after the essential control loop makes forward progress. Choose/reset limits compatible with the installed core and RP2350. Do not feed inside a hung network worker. Track reset reason when possible.
4. **Keep last-known-good snapshots.** Like cached weather/time snapshots on Postal Clock, retain a validated RTC/time and last valid schedule/sensor status. Show stale/error indicators separately rather than corrupting active control state.
5. **Concurrency only if it earns its keep.** Postal Clock used `setup()/loop()` plus `setup1()/loop1()`, a separate core-1 stack, a Pico mutex, and nonblocking try-lock to isolate work. **Do not copy this by default.** Prefer a single-core, nonblocking Fish Lamp design unless measurements show a genuine need for core 1.
6. **If adding core 1:** use the documented Arduino-Pico `setup1()/loop1()` mechanism, consider `core1_separate_stack = true`, use bounded critical sections and snapshots, and never hold locks while doing network I/O or flash writes. Bare-metal Arduino-Pico Wi-Fi APIs should run on **core 0** unless the installed core documentation explicitly says otherwise. Do not rely on `volatile` alone for synchronization.
7. **Failure isolation and recovery.** A faulty sensor, unreachable time server, failed Wi-Fi reconnect, malformed HTTP request, or telemetry outage must not crash/reboot the device in a rapid loop. Recovery must be bounded and observable.
8. **Hardware state is owned in one place.** A `LightingController`/driver applies strip and grow-light state; web handlers and scheduler only submit requests. Prevent conflicting concurrent writes and uncontrolled blinking at boot.
9. **Measure reliability, not just uptime.** Track last successful RTC/time sync, schedule update, LED application, Wi-Fi connection, sensor reading, and telemetry attempt/success. Report errors concisely through Serial and/or `/status` without leaking secrets.

## Startup and Wi-Fi provisioning (desired C++ feature)

Implement incrementally; do not assume the Python reference already has a provisioning AP.

- Boot -> mount/check LittleFS -> validate configuration -> initialize safe hardware outputs -> establish reliable time if available -> start periodic lighting loop.
- Saved Wi-Fi settings -> bounded station connection attempt -> normal network service on success; on failure **continue offline lighting** and offer recovery/setup mode without endless reboots.
- Missing/invalid settings -> **password-protected** configuration AP and a simple mobile-friendly page for Wi-Fi, location, and timezone. Never create an unprotected provisioning AP by default.
- Submit -> validate -> respond to browser -> change networking mode as needed -> test candidate settings -> persist only on success (or use a clearly defined recovery policy) -> reboot only when justified.
- **Do not assume AP+STA simultaneously works** in the installed Arduino-Pico version; inspect version/docs and code a single-mode transition that is safe without it.
- Do not require an arbitrary RSSI threshold (for example, -70 dBm) to remain connected; actual connectivity and bounded retries determine state.
- The state machine must store and expose its actual state. Avoid empty stubs, duplicated definitions, invented `ATOMICS.reboot()`, and ESP32-specific APIs. Use supported Pico APIs verified against the installed core.

## Lean target architecture (adjust to repository reality)

Suggested *responsibilities*, not an order to create ten classes at once:

```text
fish-lamp.ino         # setup/loop coordinator only
src/ConfigManager.*   # persistent config, validation, migration
src/TimeService.*     # DS3231, opportunistic NTP, UTC/local handling
src/SolarSchedule.*   # pure sunrise/sunset and offset calculations
src/LightingController.*  # single owner of NeoPixel and grow-light outputs
src/SensorManager.*   # DS18B20 async conversion, ambient/onboard ADC
src/WiFiService.*     # bounded connect/retry and provisioning AP
src/WebService.*      # existing API + setup UI, bounded processing
src/TelemetryService.* # queued/bounded temperature HTTP POST
src/AppMode.*         # actual bootstrap/setup/connect/networked state transitions if helpful
python/              # preserve as behavioral reference
```

A component can be a small module/struct rather than a class when simpler. Separate pure scheduling/time calculations so they can be tested on a desktop without attached hardware. Keep direct hardware calls in adapter/controller modules. Avoid needless cross-dependencies, hidden global mutable state, or one all-purpose `PeripheralController` class.

## Agent working rules — avoid another stalled conversion

1. **Inspect before editing.** Check the full current tree, relevant source functions, dependency versions, current compiled behavior, and actual interfaces. Explicitly separate *observed behavior* from *assumed behavior*. `python/schedule.py` is incomplete and unused by `main.py`; do not port it as active behavior.
2. **Create a feature-parity inventory** against `python/main.py` (hardware, schedules, HTTP, settings, sensors, time, Wi-Fi, UI, reporting). Mark completed / deferred / known legacy bug.
3. **Work in thin vertical slices:** first compile/safe boot and GPIO, then RTC/time, deterministic schedule, LED/grow-light outputs, persisted settings, sensors, networking/API, telemetry, and provisioning. Modify only the files needed for one slice.
4. **Complete real code, not scaffolds.** No empty functions masquerading as implementation, declarations with missing definitions, always-constant state getters, invented APIs, or silent placeholders. If intentionally incomplete, mark it explicitly and keep the firmware functioning.
5. **Compile after every slice** with `./build.ps1` from PowerShell in the repository root. Fix compiler errors before continuing. If CLI/toolchain is unavailable, say so; do not assert success.
6. **Unit-test critical pure logic** where practical: day/night boundaries, manual override expiry, DST transition, leap day, midnight, invalid RTC, no Wi-Fi, failed DS18B20, partial/corrupt config, and sunrise/sunset offsets.
7. **Do not overwrite original MicroPython** or real user settings. Make reversible, reviewable Git commits per complete slice. Exclude binary build outputs and secrets.
8. **Keep responses concise.** Report changed files, new behavior, actual compile/test result, remaining risks, and next small task. Do not dump complete source files into chat unless requested; edit files in the workspace.
9. **Avoid spending tokens on unbounded exploration.** Read the files needed for the task and use targeted search/small excerpts. If a design decision is unclear, explain the uncertainty; do not wander or perform repeated empty agent/tool cycles.
10. **Treat building as a floor, not the finish line.** Hardware validation is required before claiming the Fish Lamp is reliable.

## Acceptance checklist — hardware validation required

- Cold boot, reset, power interruption, corrupted/missing settings, and watchdog recovery all land in a documented and safe state.
- LED count = 85, GPIO and GRB color order correct; per-pixel colors and brightness survive reboot and do not flicker unexpectedly.
- Grow-light output remains in a safe known state during startup and follows schedule/overrides with correct polarity.
- Sunrise/sunset, time zones, DST, midnight, manual override expiry, and next-day transitions work in scripted tests and real hardware checks.
- **With Wi-Fi disabled for 24+ hours**, DS3231 timekeeping and automatic lighting continue; reconnecting cannot interrupt them.
- DS18B20 disconnected/reconnected, bad ADC reads, failed DNS/NTP, unresponsive HTTP clients, and unavailable telemetry server do not freeze the light-control loop.
- Web API status and control routes work with the existing browser UI or with documented compatible replacements.
- Config saves survive power cycling; old settings migrate safely; no password ever appears in diagnostics.
- No unexpected heap growth, frequent flash writes, deadlocks, or watchdog reset loops during long-running operation.
- Record measured durations/temperatures/resource use and actual on-device results rather than assuming success from simulation.

## Reference documentation

- Arduino-Pico Wi-Fi: https://arduino-pico.readthedocs.io/en/latest/wifi.html
- Arduino-Pico multicore: https://arduino-pico.readthedocs.io/en/latest/multicore.html
- Arduino-Pico RP2040/Pico helper and watchdog: https://arduino-pico.readthedocs.io/en/latest/rp2040.html
- Postal-Clock: https://github.com/michaelmcdaniel/postal-clock

**When documentation and installed core behavior differ, the installed version and a minimal reproducible build/test take precedence.**
