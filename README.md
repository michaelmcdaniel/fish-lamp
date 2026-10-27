# Fish Lamp — offline lighting and first-boot setup

This is the first functional C++ slice for Pico 2 W and Pico WH, not the completed appliance.
It implements safe outputs, DS3231-backed offline time, solar scheduling, validated
configuration storage, an open Wi-Fi setup network, and USB Serial controls. Hardware reliability has **not**
been established. See [feature parity](docs/FEATURE-PARITY.md) and
[verification](docs/VERIFICATION.md) for scope and remaining work.

## Build and tests

Verified dependencies: Arduino-Pico **6.0.0**, ArduinoJson **7.4.3**, and
Adafruit NeoPixel **1.15.2**, OneWire **2.3.8**, and U8g2 **2.36.19** (OLED).
Arduino CLI can install missing libraries with
`arduino-cli lib install "ArduinoJson@7.4.3" "Adafruit NeoPixel@1.15.2" "OneWire@2.3.8" "U8g2@2.36.19"`.
`build-pico2w.ps1` targets Pico 2 W: `rp2040:rp2040:rpipico2w:flash=4194304_262144`.
`build-picowh.ps1` targets Pico WH using `rp2040:rp2040:rpipicow:flash=2097152_262144`:
2 MB flash with 256 KiB reserved for LittleFS. Its upload file is
`build/picowh/fish-lamp.ino.uf2`; the Pico 2 W output is
`build/pico2w/fish-lamp.ino.uf2`.

On a new Windows x64 computer, run `./setup-tools.ps1` once with Internet access.
It installs Arduino CLI **1.5.1** if missing (verifying the official archive's
SHA-256 checksum), registers the [Arduino-Pico board index](https://arduino-pico.readthedocs.io/en/latest/install.html#installing-via-arduino-cli), and installs the exact core/library versions above.
An existing CLI on PATH or in Program Files is reused. Otherwise the CLI goes in
`%LOCALAPPDATA%\Programs\Arduino CLI`; build/test scripts find it automatically,
without changing PATH. The board tools and libraries use Arduino's user directories.
Re-running setup reuses installed dependencies. Visual Studio C++ tools are needed
only for host tests, not for the firmware build.

`FishLamp-Setup` is an **open network with no password**, as requested by the
owner. No setup-key generation is required to build. Existing ignored
`src/SetupSecrets.h` and `setup-access.txt` files are unused by the firmware;
`tools/new-setup-key.ps1` is a legacy helper and does not change the open AP.
Home Wi-Fi credentials still belong in the setup form, and the setup session
token is still required when submitting configuration.

```powershell
./setup-tools.ps1 # first-time toolchain installation
./build-pico2w.ps1
./build-picowh.ps1 # Pico WH instead of Pico 2 W
./test.ps1
./tests/test-migration.ps1
```

Where Windows blocks unsigned scripts, use a process-only override:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./setup-tools.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File ./build-pico2w.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File ./test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File ./tests/test-migration.ps1
```

Host tests require Visual Studio C++ tools. `test.ps1` accepts `-ArduinoJson`
with the library's `src` path to override automatic discovery from Arduino CLI's
configured sketchbook directory.
Generated firmware and native test binaries stay in ignored build directories.

## Configure on first boot

### Hardware switches / bench deployment

Edit **src/Devices.h**, then rebuild and upload. `FISH_LAMP_BENCH=1` mocks all
external gadgets while retaining real Wi-Fi, internal temperature and the onboard
status LED. Its default is 0 (real hardware). Each individual switch can override
that profile: **1 = real, 0 = mock**. These are firmware settings, not saved JSON.

| Switch | Device / behavior when mocked |
|---|---|
| `FISH_LAMP_STRIP` | GPIO 7 strip: accepts colors; logs changed pixel index and applied RGB after brightness |
| `FISH_LAMP_GROW` | GPIO 8: accepts switching requests; logs changes |
| `FISH_LAMP_RTC` | DS3231: starts at `mockRtcUtc`, advances in RAM; time writes accepted and logged |
| `FISH_LAMP_WATER` | DS18B20: `mockWaterF`, initially 77 °F |
| `FISH_LAMP_AMBIENT` | Photoresistor: `mockAmbientPercent`, initially 50% |
| `FISH_LAMP_CPU_TEMP` | Internal ADC temperature: `mockCpuC`, initially 30 °C |
| `FISH_LAMP_STATUS_LED` | Onboard LED: logs on/off changes |
| `FISH_LAMP_WIFI` | Reports disabled/disconnected; no application scans, AP or station connections |
| `FISH_LAMP_OLED` | 0 disables the OLED completely; no buffer, library requirement or display I2C access |

Mock RTC defaults to 2026-01-01 12:00 UTC at **every restart**, including after
setup. Change `mockRtcUtc` to exercise a particular schedule date/time. A setup
time write affects RAM only; mock time is not a replacement for a battery RTC.
RTC health stays false and diagnostics identify the mocked devices explicitly.
Keep Wi-Fi enabled to configure a blank Pico. With Wi-Fi mocked, valid settings
must already exist in LittleFS or lights remain safely off. The core may still
initialize its onboard radio; the switch disables application networking.
Flash storage, USB Serial, watchdog and the Pico itself remain real.

### Optional OLED diagnostics

The display matches [Postal Clock's driver](https://github.com/michaelmcdaniel/postal-clock/blob/main/display.h):
a **SH1106 128×64** 1.3-inch I2C OLED, and uses
U8g2 for its static framebuffer and 5×8 font. It is enabled by default with
`FISH_LAMP_OLED=1`, independently of the bench profile. Set it to 0 in
`src/Devices.h` to build without the display or U8g2 dependency.

| OLED connection | Pico W/WH or Pico 2 W |
|---|---|
| SDA | GPIO 2 (physical pin 4) |
| SCL | GPIO 3 (physical pin 5) |
| VCC | 3V3 (OUT), physical pin 36; use a 3.3 V compatible module |
| GND | GND, for example physical pin 3 |

The OLED uses **Wire1 / I2C1**. The DS3231 remains on **Wire / I2C0, GPIO 16/17**;
the display never reinitializes the RTC bus. Pull-ups must go to 3.3 V.
GPIO 0/1 cannot provide a second independent hardware bus alongside the RTC's
GPIO 16/17. OLED pins and its **7-bit** address are centralized in `src/Devices.h`.
The default address is **0x3C**; change `devices::oledAddress` to **0x3D** if that
is how the module is strapped. Startup checks the configured address by ACK,
not by assuming a particular address or scanning the RTC bus. Controller identity
and the physical address still require on-device confirmation.

After a short startup message, eight rows show Wi-Fi state, RTC health/time,
water temperature in °F, ambient percentage, applied grow output, RGB mode and
brightness, uptime, and the last observed warning. `L` means configured local
time; `Z` means UTC when settings are unavailable. `RTC FAIL` with a time means
the software clock is preserving the last validated time. `MOCK`/`M` identifies
simulated devices and `STALE` identifies retained readings after a failed sample.
RGB `B:min-max` is the **applied per-pixel brightness range, 0–255**, rather than
an invented global brightness. `*` marks an RGB manual override; grow overrides
are marked `MAN`. `Last:` retains the most recently observed warning after
recovery; simultaneous faults have a deterministic display priority. Long warning
messages cycle through numbered pages every three seconds. Setup failures also
print their complete message to USB Serial. When connected, the first row cycles
every five seconds through SSID, IP address and RSSI in dBm. Long SSIDs scroll;
unsupported font characters appear as `?`. No password, setup key or endpoint is shown.

`StatusDisplay` observes existing state every 500 ms and sends only changed
rows, one bounded I2C transaction per loop (16 framebuffer bytes per data chunk).
Initialization waits use `millis()`, with no `delay()` or full-frame `sendBuffer()`.
The OLED bus uses 100 kHz and a 5 ms transaction timeout. A missing OLED, NACK or
transfer error disables display I/O until restart and queues one Serial warning;
it never retries forever or changes lamp state. Disabling the display also stops
RSSI sampling added for it. Actual bus timings, glyph layout/orientation, address,
and operation with a disconnected/stuck OLED must be checked on hardware.

If OneDrive leaves the installed U8g2 source files unreadable, extract the official
U8g2 2.36.19 Arduino library archive into `build/dependencies/`. Both build scripts
automatically prefer `build/dependencies/U8g2-2.36.19` when present, so their normal
no-argument invocation works without replacing the shared installation. This clean
copy is already available in this workspace. An explicit path overrides automatic
selection:

```powershell
./build-picowh.ps1 -U8g2Path ./build/dependencies/U8g2-2.36.19
./build-pico2w.ps1 -U8g2Path ./build/dependencies/U8g2-2.36.19
```

Use `src/Devices.h` for switches and pin edits; generated copies under build
directories are overwritten by Arduino CLI.

Open Serial Monitor at 115200 for `mock ...` output and periodic sensor snapshots.
Output requests are logged on changes, not every tick. Logs use a fixed 4 KiB
queue, drain at most 32 bytes per loop, and drop records rather than block when
full (`mockDropped` reports the count). No Wi-Fi credentials are logged.

Real water sensing supports **one externally powered DS18B20** on GPIO 26, with
the required pull-up. Parasite-power and multiple sensors on that bus are not
supported. Conversion is asynchronous (at least 750 ms); samples/retries occur
once per minute. CRC/family/range failures retain the last valid value and mark
it unhealthy. Ambient percentage is sampled every **500 ms** on its own timer;
averaged CPU temperature remains on the minute timer. The OLED uses the cached
ambient reading at its 500 ms refresh cadence. `valid=0` means no usable reading; never interpret its numeric default
as a measurement. The onboard status LED blinks every 250 ms during a station
connection attempt, stays on while connected, and is off in setup AP, retry-wait
and offline modes. Normal-operation HTTP sensor reporting remains deferred.

### Provisioning steps

1. Build and upload the firmware for your Pico. On missing/invalid settings or a
   saved BOOTSEL setup request,
   it scans nearby 2.4 GHz networks for up to eight seconds, then starts the
   open **FishLamp-Setup** network. Join without entering a password.
2. Join that network from your phone/computer and open **http://192.168.4.1/**.
   Stay connected if the device warns that this Wi-Fi has no Internet. Wildcard
   DNS and HTTP captive-probe redirects may open the setup window automatically;
   the operating system decides whether to show it. If it does not, enter that
   address manually in a full browser. HTTPS probes cannot be intercepted.
   All page assets are local.
3. Choose a nearby SSID from the dropdown, **Manual** to show a name textbox,
   or **None** for offline lighting. None hides the password field. The dropdown
   lists up to 24 distinct 2.4 GHz networks, strongest signal first. **Refresh**
   beside the SSID label fetches `/networks` without reloading the page and
   preserves the selection. It reloads the cached pre-AP scan; it does not rescan
   the radio or disconnect clients. List fetches retry at most three times.
   The API reports status, count and numeric start error, and Serial logs those
   diagnostics once per scan without printing SSIDs.
   The browser fills the current clock and its detected standard UTC offset/DST
   rule automatically for fixed-offset and US-rule zones. Other seasonal rules
   open Advanced for review; the firmware still supports `none` and `us` only.
   **Advanced overrides** exposes offset/rule and a UTC clock override.
   Home Wi-Fi asks for a five-digit **US ZIP code**; **None** instead shows
   required latitude/longitude fields. Offline device-location lookup is optional
   and starts only when its button is pressed, with normal accuracy and an
   eight-second timeout. Ordinary HTTP setup normally blocks browser geolocation;
   enter coordinates manually in that case. Choose colors/brightness and offsets.
   An optional local JSON import preserves per-pixel colors and schedule settings.
4. Select **Save settings & restart**. For home Wi-Fi, the browser receives an
   acknowledgement before AP mode stops. The lamp attempts station connection
   for up to 20 seconds. A missing compiled ZIP may add up to six seconds for a
   location lookup. On success, it initializes/verifies the RTC, saves and
   verifies settings, then restarts. Offline mode skips the connection test and
   reports successful saving directly before restarting.
5. If network testing, RTC verification or storage fails, setup remains available
   or returns automatically. Rejoin FishLamp-Setup and reload for the error.
   The browser's initial network-test acknowledgement is not a saved confirmation.

The firmware explicitly stops the AP before joining home Wi-Fi, resets the
station interface to DHCP, and accepts only a station connection as success.
Failed tests distinguish authentication, missing 2.4 GHz network and DHCP errors
when the radio provides that information. Read the numbered OLED error pages,
USB Serial, or rejoin the setup AP to read the complete message in the browser.

### ZIP coordinate lookup

The firmware includes 41,488 US ZIP coordinates from the
[GeoNames postal dataset](https://download.geonames.org/export/zip/), converted
to integer coordinates at 0.00001-degree precision and searched in flash. These
are approximate postal-area locations, not device GPS readings. See
[ZIP data attribution](docs/ZIP-DATA.md). The table uses 497,856 bytes of flash
and requires no filesystem download or runtime heap allocation.

Only ZIPs missing from that table use the free
[Zippopotam.us API](https://api.zippopotam.us/), after station connection. One
asynchronous DNS/TCP/HTTP request has a six-second total deadline, a 4 KiB
response limit, bounded JSON storage and no retries. This public-coordinate
request sends only the ZIP and uses HTTP; it sends no Wi-Fi credentials. If the
lookup fails, the saved location is **35.84562, -86.39027**. Network loss during
lookup also saves this fallback. Location failure does not reject working Wi-Fi
or stop lighting. Resolved/fallback coordinates persist in the existing verified
config save, so subsequent boots do not query the API. Offline setup never queries it.

After a successful restart, saved settings are loaded and lighting starts using
the RTC. **Setup does not repeat on every power-up.** The AP is absent on a normal
configured boot. Station failures later leave lighting operating offline, with
20-second connection attempts separated by 60-second retry waits. Changing a
previously saved configuration through a normal-operation web UI is still deferred.

### Enter setup using BOOTSEL and a power cycle

With the lamp running, hold the onboard **BOOTSEL** button until the OLED says
**Setup on next boot** (or Serial acknowledges the armed request). **Keep it held
while unplugging power.** Then release BOOTSEL and reconnect power normally.
Releasing it while the firmware is still running cancels the request; let the
OLED message clear or wait for the Serial cancellation acknowledgement before
unplugging. Holding BOOTSEL during reconnection enters the ROM UF2 bootloader
instead of running this firmware.

The core's supported BOOTSEL reader is sampled every 20 ms, with two pressed
samples required to debounce. ConfigManager records the Boolean request in the
small `/setup.request` marker beside `config.json`, using a verified temporary
write and atomic rename. Release removes the marker. This avoids rewriting the
85-pixel configuration on each press/release. There is at most one set/clear
operation per hold; filesystem failures are reported. A marker left by loss of
power is checked at boot, so setup starts even with valid saved settings. The
saved lighting schedules continue while that setup AP is open. Successful setup
saving also clears the request. On-device power-cut and release timing still
require validation.

For UI-only development, `node tools/preview-setup.cjs` serves a loopback-only
preview with synthetic SSIDs and no settings writes. Its Refresh response adds a
third synthetic network. `node tests/test_setup_page.cjs` checks the embedded
browser script using Node built-ins, including location permission failure,
clock/zone defaults and overrides, SSID refresh, Manual and None behavior.

## Runtime policy

* GPIO 8 is driven LOW first; all 85 GPIO 7 GRB pixels are cleared. The grow
  output retains HIGH=on. Physical polarity/color order still need confirmation.
* An 8-second hardware watchdog starts before filesystem/time initialization.
  Only completion of essential control feeds it. All Wi-Fi runs on core 0 after
  essential control, with `beginNoBlock` connection attempts. There are no threads.
  USB status transmits at most 32 available bytes per iteration. Low-level Wi-Fi
  initialization and flash-operation durations still require hardware measurement.
* LittleFS auto-format is disabled. Missing/corrupt settings or a failed mount
  keep RGB completely black and grow LOW while setup runs. Valid `/config.bak`
  is tried after invalid/missing `/config.json`. Explicit setup submission may
  format an unreadable filesystem; merely booting never formats it. Settings
  write to `/config.tmp`, are read back byte-for-byte and validated, then replace
  the primary using LittleFS rename. An existing valid primary is first renamed
  to backup; a corrupt primary never replaces a good backup. Boot ignores temp
  files and uses backup if power failed between renames. No routine loop saves.
* DS3231 uses SDA 16 / SCL 17, 100 kHz, and 5 ms transaction timeouts.
  A read attempt comprises at most four I2C operations, nominally at most 20 ms
  of timeout waits plus overhead. Reads occur at boot and once per minute.
  BCD, dates, hour formats, century bit and oscillator-stop flag are checked.
  Supported RTC years are 2000–2099. An invalid first read leaves outputs off.
* After a successful RTC read, failures retain the software UTC timebase;
  `rtc=0` reports holdover. Holdover continues indefinitely until recovery, with
  accumulating oscillator error. A later valid read corrects software time.
  Only an explicit setup submission writes the RTC and clears its oscillator-stop
  flag, followed by readback verification. Routine polling never writes the RTC.
* Schedules use explicit coordinates and timezone configuration. Windows are
  inclusive at on and exclusive at off. Yesterday/today/tomorrow are cached to
  support offsets spanning midnight. Cache refresh follows the local date,
  including clock corrections that change it; a correction within the same
  date immediately reevaluates the existing windows. Config is immutable after boot.
* RGB selects day/night arrays; grow is independent. Polar days with no solar
  event disable that day's day/grow window; night RGB still applies. Empty
  windows are off. Overlapping windows from adjacent days are combined.
* Brightness is per pixel, scaled with integer rounding. Hardware writes occur
  only when the resulting pixel/output state changes. No scheduler flash writes.

Solar calculations use the [NOAA fractional-year approximation](https://gml.noaa.gov/grad/solcalc/solareqns.PDF)
with a 90.833-degree zenith. They are suitable for approximate lighting events,
not precision astronomical timing. Independent reference checks at the actual
configured location remain part of commissioning.

## Configuration and migration

The flash layout reserves **262,144 bytes** for LittleFS. Each settings file is
limited to **16,384 bytes**, nesting to four levels and JSON allocation to 64 KiB
including allocation headers. Primary and backup easily fit the partition.
Both arrays require exactly 85 entries of four integer bytes `[r,g,b,brightness]`.

Existing six offset names are retained. Sunrise/sunset offsets are **seconds**
in [-43,200, 43,200]; max offsets are local seconds after midnight in [0, 86,400].
Off is the earlier of offset sunset and the local max limit.

These additional fields must be explicitly supplied:

| Field | Meaning |
|---|---|
| `schema-version` | Integer `1` |
| `rtc-utc-confirmed` | Boolean `true`, only after verifying RTC convention |
| `latitude`, `longitude` | Numeric degrees, north/east positive |
| `zip-code` | Optional five-digit US ZIP, only with home Wi-Fi; old files without it remain valid |
| `location-source` | Setup records `manual`, `compiled-zip`, `api-zip` or `fallback` for the saved coordinates |
| `timezone-offset-minutes` | Standard UTC offset, east positive, -720 through 840 |
| `timezone-dst` | `none` or `us`; US rules from 2007 onward, not an IANA timezone database |

No location, UTC convention, or timezone is inferred from Python defaults.
US spring gaps advance wall times by one hour; repeated fall wall times choose
the earlier occurrence. This also defines ambiguous absolute-off limits.
Other DST jurisdictions are not supported yet; do not label them `us`.

`tools/migrate-config.ps1` reads a **copy** of legacy settings, requires explicit
location/timezone and `-RtcUtcConfirmed`, validates with the same C++ parser, and
creates a new output file without overwriting either existing file. Run host tests
first to build its validator. Required parameters are `-InputPath`, `-OutputPath`,
`-Latitude`, `-Longitude`, `-TimezoneOffsetMinutes`, and `-Dst`.
Use an ignored `data/config.json` output path (create `data` first).
It preserves original arrays, offsets, dates and additional fields. Credentials
and endpoint values are not displayed. Legacy values outside the accepted ranges
are rejected for explicit correction, not silently clamped.

`wifi-sid`, `wifi-pwd`, `temperature-url`, `createDate`, and `modDate` remain in the
source/migrated JSON. SSID/password are retained privately for station connection;
the telemetry URL remains unused. Wi-Fi passwords must be empty for an open
network, 8–63 printable ASCII characters, or a 64-digit hexadecimal key. This
adds validation for previously unused network fields. Credentials are never
returned by the setup endpoints or included in diagnostics. The setup page now
initializes storage and RTC without a separate filesystem upload or USB import.
Existing MicroPython flash layout is not assumed compatible with the Arduino
partition; save a copy of settings before replacing an existing installation.

The setup-only `setup-utc` field is an integer browser UTC epoch. It is validated
on submission and ignored on later boots; saved time never replaces RTC time.
The firmware accounts for elapsed network-testing time before writing the RTC.

## USB Serial interface

Open Serial at 115200; the controller never waits for a host. Send single bytes:

| Byte | Request |
|---|---|
| `s` | Status snapshot |
| `l` / `L` | Day RGB / force RGB completely black |
| `g` / `G` | Grow on / grow off |
| `a` | Cancel both manual overrides; restore schedule |

Manual commands require valid config/time. Overrides expire at **one hour of
monotonic time or the next schedule boundary, whichever comes first**. This
intentionally replaces inconsistent Python `min`/`max` rules. Force-off has
priority over day/night RGB until expiry. Requests apply on the next loop.

Status is emitted every 30 seconds when USB can accept it. `config`: 0 mount
failed, 1 invalid, 2 primary loaded, 3 backup loaded. `time` means a validated
timebase exists; `rtc` means the latest read succeeded; `solar` means today's
events exist. `day`, `grow`, and `forcedOff` describe desired state; `strip`
reports driver initialization. Ages report milliseconds since last success,
calculation or hardware change (wrap-safe intervals, not persistent timestamps).
Age fields before any success are not meaningful; inspect validity fields first.
`maxControlUs` covers essential work only, excluding diagnostics. `reset` is the
installed core's reset reason enum. `network` reports: 0 disabled, 1 starting AP,
2 setup AP, 3 AP failure/backoff, 4 testing candidate Wi-Fi, 5 restart pending,
 6 station connecting, 7 station connected, 8 station retry, 9 scanning. No credentials or
configuration values print.

HTTP port 80 serves setup only: `GET /`, `GET /session`, `GET /networks`,
`POST /configure`. Network discovery uses a fixed-capacity CYW43 callback and
finishes station scanning before starting the AP; AP+STA is not required.
Requests require the fixed AP host and a session token for configuration changes;
foreign origins and chunked/ambiguous body framing are rejected. One request is
serviced at a time, with a listen backlog of one, 2 KiB headers, 16 KiB bodies,
256-byte per-tick receive/send budgets, a 2-second inactivity limit and 15-second
total limit. Client writes use a 5 ms timeout; close uses a 1 ms ACK wait. No
AP+STA overlap is used. The legacy normal-operation API/UI is still deferred.
