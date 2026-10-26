# Fish Lamp — initial offline firmware

This is the first functional C++ slice for Pico 2 W, not the completed appliance.
It implements safe outputs, DS3231-backed offline time, solar scheduling, validated
configuration loading, and USB Serial controls. Hardware reliability has **not**
been established. See [feature parity](docs/FEATURE-PARITY.md) and
[verification](docs/VERIFICATION.md) for scope and remaining work.

## Build and tests

Verified dependencies: Arduino-Pico **6.0.0**, ArduinoJson **7.4.3**, and
Adafruit NeoPixel **1.15.2**. Arduino CLI can install missing libraries with
`arduino-cli lib install "ArduinoJson@7.4.3" "Adafruit NeoPixel@1.15.2"`.
The existing `build.ps1` and its board/flash selection are unchanged.

```powershell
./build.ps1
./test.ps1
./tests/test-migration.ps1
```

Where Windows blocks unsigned scripts, use a process-only override:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File ./test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File ./tests/test-migration.ps1
```

Host tests require Visual Studio C++ tools. `test.ps1` accepts `-ArduinoJson`
with the library's `src` path if it is not in the default Documents location.
Generated firmware and native test binaries stay in ignored build directories.

## Runtime policy

* GPIO 8 is driven LOW first; all 85 GPIO 7 GRB pixels are cleared. The grow
  output retains HIGH=on. Physical polarity/color order still need confirmation.
* An 8-second hardware watchdog starts before filesystem/time initialization.
  Only completion of essential control feeds it. There are no threads or Wi-Fi
  calls. USB status transmits at most 32 available bytes per iteration.
* LittleFS auto-format is disabled. Missing/corrupt settings or a failed mount
  keep RGB completely black and grow LOW. Valid `/config.bak` is tried after
  invalid/missing `/config.json`. No settings are written by this release.
* DS3231 uses SDA 16 / SCL 17, 100 kHz, and 5 ms transaction timeouts.
  A read attempt comprises at most four I2C operations, nominally at most 20 ms
  of timeout waits plus overhead. Reads occur at boot and once per minute.
  BCD, dates, hour formats, century bit and oscillator-stop flag are checked.
  Supported RTC years are 2000–2099. An invalid first read leaves outputs off.
* After a successful RTC read, failures retain the software UTC timebase;
  `rtc=0` reports holdover. Holdover continues indefinitely until recovery, with
  accumulating oscillator error. A later valid read corrects software time.
  The firmware never writes the RTC or clears its oscillator-stop flag.
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
source/migrated JSON. Networking fields are unused at runtime in this release.
The parser validates optional network strings' types/lengths but does not retain
them. Firmware saves, atomic replacement, USB import, filesystem upload and RTC
setting are deferred. A compatible LittleFS image containing the validated
configuration and a correctly initialized UTC RTC are **commissioning prerequisites**;
building/flashing the sketch alone does not provision them. Existing MicroPython
flash layout is not assumed compatible with this Arduino partition.

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
installed core's reset reason enum. No credentials or configuration values print.

HTTP port 80 and the existing browser UI are **not implemented in this slice**.
