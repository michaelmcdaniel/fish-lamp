#line 1 "C:\\Users\\mmcda\\OneDrive\\Code\\pico\\fish-lamp\\docs\\FEATURE-PARITY.md"
# Feature-parity inventory

Reference inspected: active `python/main.py`, `suncalc.py`, `settings.py`, and
`rtc_lib.py`. `python/schedule.py` remains unused and unported. Python files and
real settings have not been changed. “Implemented” below means code and listed
desktop checks, not hardware acceptance.

| Area | Initial release | Remaining / compatibility decision |
|---|---|---|
| GPIO 7 / 85 GRB pixels | Implemented, per-pixel RGB and brightness | Physical order, timing, electrical validation pending |
| GPIO 8 grow output | Implemented, HIGH on and safe LOW boot | Confirm physical switching polarity |
| Day/night arrays | Implemented, strict 85 × 4 byte validation | No brightness/color HTTP edits yet |
| Independent solar schedules | Implemented, all six legacy keys and seconds | Actual-location comparison with reference solar events pending |
| Timezone / DST / midnight | Implemented explicit UTC, fixed offset or current US rules | Other DST rules deferred; no geographic defaults copied |
| Manual overrides / force off | Implemented via bounded Serial commands | One hour or next boundary; documented correction to legacy inconsistency |
| DS3231 | Implemented read/validate/holdover/recovery and browser-time initialization | On-device bus timeout and readback measurements pending |
| NTP | Deferred | Must be optional, bounded, and update RTC consistently |
| LittleFS / settings | Implemented validation, backup fallback, migration and verified temp/rename save | Actual power-cut recovery tests pending; format only on explicit setup submit |
| Water DS18B20 GPIO 26 | Async 750 ms conversion, minute sampling, CRC/family/range checks, last-valid snapshot, mock switch | One externally powered sensor; physical fault/reconnection tests pending |
| Ambient GPIO 27 | 12-bit ADC normalized to 0–100%, mock switch | Physical ADC validation pending |
| Internal CPU temperature | Supported core internal ADC API, eight-sample average, mock switch | Separate Celsius reading; physical validation pending |
| Status LED | On when config/time usable; mock switch | Physical indication pending |
| Device selection | Individual real/mock compile-time switches; external-device bench profile | Mock outputs use bounded debug queue; mock RTC resets at boot |
| Wi-Fi station | Implemented nonblocking connection/retry states | Actual outage/reconnection timing tests pending |
| Protected provisioning AP | Implemented for missing/invalid config, local mobile page, test/save/restart | Private per-build key; physical AP and restart tests pending |
| Setup network discovery | Bounded pre-AP scan; editable SSID combo; strongest 24 distinct networks cached | Manual hidden-network entry; physical scanning pending |
| HTTP and Vue UI | Setup-only page implemented | Legacy normal-operation API and Vue UI remain deferred |
| Telemetry POST | Deferred | Preserve Fahrenheit `temperature`; document that humidity/pressure were placeholders |
| Watchdog and diagnostics | Implemented | USB snapshot, reset cause, progress timings; fault-injection hardware tests pending |

Deferred normal-operation HTTP contract: legacy `GET /`, `/status`, `/lights/on`, `/lights/off`,
`/lights/brightness/{value}`, `/lights/{index}/{hexcolor}`,
`/lights/{start}-{end}/{hexcolor}`, `/lights/{hexcolor}`, `/grow-light/on`,
`/grow-light/off`, `/grow-light`, `/log/temp[/on|/off]`, and
`/log/schedule[/on|/off]`. Future `/status` must retain `lightsOn`, `growlightOn`,
`lights`, `schedule`, `growlightDelay`, `lightsDelay`, `waterTemp`, and `ambient`.
The setup page is a new locally served interface, not the legacy Vue page. Its
mobile browser submission was tested against a local preview server; physical
AP serving remains unverified. Normal-operation browser compatibility is deferred.

## Observed legacy defects not copied

* Hand-written timezone/DST logic and date arithmetic are replaced with tested
  UTC/local boundaries; no adjustments are performed twice.
* Force-off no longer competes with schedule branches or disappears after one tick.
* Grow output is explicitly off outside a valid window, including before sunrise.
* An all-black configured day array is respected instead of being overwritten
  with full white and saved from the scheduler.
* Routine scheduling never writes flash or repeatedly redraws unchanged pixels.
* Temperature endpoint/sensor exceptions in Python could ultimately reboot the
  controller. These services are deferred; their future implementations must
  preserve control and last-valid sensor state instead.
* Python's `waterTemp` update depended on successful network-related branches.
  Future sensor ownership must be independent of POST success.

## Next vertical slice

Validate first-boot AP setup, RTC initialization, saving/restart and power
interruption and real sensor/output adapters on hardware. Then add the normal-operation
API/UI and configuration maintenance, and telemetry. Build and fault-test each slice.
