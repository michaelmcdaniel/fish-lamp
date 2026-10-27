# Fish Lamp — offline lighting and first-boot setup

This is the first functional C++ slice for Pico 2 W, not the completed appliance.
It implements safe outputs, DS3231-backed offline time, solar scheduling, validated
configuration storage, a protected Wi-Fi setup page, and USB Serial controls. Hardware reliability has **not**
been established. See [feature parity](docs/FEATURE-PARITY.md) and
[verification](docs/VERIFICATION.md) for scope and remaining work.

## Build and tests

Verified dependencies: Arduino-Pico **6.0.0**, ArduinoJson **7.4.3**, and
Adafruit NeoPixel **1.15.2**, and OneWire **2.3.8**. Arduino CLI can install missing libraries with
`arduino-cli lib install "ArduinoJson@7.4.3" "Adafruit NeoPixel@1.15.2" "OneWire@2.3.8"`.
The board/flash selection remains `rp2040:rp2040:rpipico2w:flash=4194304_262144`.

On a new Windows x64 computer, run `./setup-tools.ps1` once with Internet access.
It installs Arduino CLI **1.5.1** if missing (verifying the official archive's
SHA-256 checksum), registers the [Arduino-Pico board index](https://arduino-pico.readthedocs.io/en/latest/install.html#installing-via-arduino-cli), and installs the exact core/library versions above.
An existing CLI on PATH or in Program Files is reused. Otherwise the CLI goes in
`%LOCALAPPDATA%\Programs\Arduino CLI`; build/test scripts find it automatically,
without changing PATH. The board tools and libraries use Arduino's user directories.
Re-running setup reuses installed dependencies. Visual Studio C++ tools are needed
only for host tests, not for the firmware build.

The private setup key has already been generated in this workspace. On a fresh
checkout, first run `tools/new-setup-key.ps1` **once**. It creates ignored
`src/SetupSecrets.h` and `setup-access.txt`; builds intentionally fail without
the header rather than creating an open AP. Keep these files private and reuse
them for subsequent builds. The generator never overwrites existing access files.

```powershell
./setup-tools.ps1 # first-time toolchain installation
./build.ps1
./test.ps1
./tests/test-migration.ps1
```

Where Windows blocks unsigned scripts, use a process-only override:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./setup-tools.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File ./build.ps1
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

Mock RTC defaults to 2026-01-01 12:00 UTC at **every restart**, including after
setup. Change `mockRtcUtc` to exercise a particular schedule date/time. A setup
time write affects RAM only; mock time is not a replacement for a battery RTC.
RTC health stays false and diagnostics identify the mocked devices explicitly.
Keep Wi-Fi enabled to configure a blank Pico. With Wi-Fi mocked, valid settings
must already exist in LittleFS or lights remain safely off. The core may still
initialize its onboard radio; the switch disables application networking.
Flash storage, USB Serial, watchdog and the Pico itself remain real.

Open Serial Monitor at 115200 for `mock ...` output and periodic sensor snapshots.
Output requests are logged on changes, not every tick. Logs use a fixed 4 KiB
queue, drain at most 32 bytes per loop, and drop records rather than block when
full (`mockDropped` reports the count). No Wi-Fi credentials are logged.

Real water sensing supports **one externally powered DS18B20** on GPIO 26, with
the required pull-up. Parasite-power and multiple sensors on that bus are not
supported. Conversion is asynchronous (at least 750 ms); samples/retries occur
once per minute. CRC/family/range failures retain the last valid value and mark
it unhealthy. Ambient percentage and averaged CPU temperature are separate
readings. `valid=0` means no usable reading; never interpret its numeric default
as a measurement. The onboard status LED is on when configuration and time are
usable. Normal-operation HTTP sensor reporting remains deferred.

### Provisioning steps

1. Build and upload the firmware to the Pico 2 W. On missing or invalid settings,
   it scans nearby 2.4 GHz networks for up to eight seconds, then starts the
   password-protected **FishLamp-Setup** network. Find its password
   in the local ignored `setup-access.txt` file. Credentials never print to Serial.
2. Join that network from your phone/computer and open **http://192.168.4.1/**.
   Stay connected if the device warns that this Wi-Fi has no Internet. There is
   no captive DNS redirect; enter the address explicitly. All page assets are local.
3. Choose home Wi-Fi from the editable network combo box (or choose offline
   lighting). It lists up to 24 distinct networks, strongest signal first, from
   the scan before AP startup. Hidden names can be typed manually, including
   when scanning fails. The list is cached until the next setup boot.
   Enter coordinates, standard timezone,
   DST rule, colors/brightness and independent schedule offsets/off limits.
   Confirm the phone/computer's clock is correct; its UTC epoch sets the DS3231.
   An optional local JSON import preserves per-pixel colors and schedule settings.
4. Select **Save settings & restart**. For home Wi-Fi, the browser receives an
   acknowledgement before AP mode stops. The lamp attempts station connection
   for up to 20 seconds. On success, it initializes/verifies the RTC, saves and
   verifies settings, then restarts. Offline mode skips the connection test and
   reports successful saving directly before restarting.
5. If network testing, RTC verification or storage fails, setup remains available
   or returns automatically. Rejoin FishLamp-Setup and reload for the error.
   The browser's initial network-test acknowledgement is not a saved confirmation.

After a successful restart, saved settings are loaded and lighting starts using
the RTC. **Setup does not repeat on every power-up.** The AP is absent on a normal
configured boot. Station failures later leave lighting operating offline, with
20-second connection attempts separated by 60-second retry waits. Changing a
previously saved configuration through a normal-operation web UI is still deferred.

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
