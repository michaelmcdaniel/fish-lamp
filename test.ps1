param([string]$ArduinoJson)
$ErrorActionPreference = 'Stop'
if (-not $ArduinoJson) {
    $cli = & "$PSScriptRoot\tools\find-arduino-cli.ps1"
    $sketchbookJson = & $cli config get directories.user --json
    if ($LASTEXITCODE -ne 0) { throw 'Could not read Arduino CLI configuration.' }
    $sketchbook = $sketchbookJson | ConvertFrom-Json
    $ArduinoJson = Join-Path $sketchbook 'libraries\ArduinoJson\src'
}
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio C++ tools are required for host tests.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools are required for host tests.' }
if (-not (Test-Path -LiteralPath "$ArduinoJson\ArduinoJson.h")) { throw 'Pass -ArduinoJson with the installed library src directory.' }
$root = $PSScriptRoot
$out = Join-Path $root 'test-build'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$runner = Join-Path $out 'run.cmd'
@"
@echo off
call "$vs\VC\Auxiliary\Build\vcvars64.bat" >nul
if %errorlevel% neq 0 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /W4 /I"$root\tests\fakes" /I"$root\src" /I"$ArduinoJson" "$root\tests\test_main.cpp" "$root\tests\test_setup.cpp" "$root\src\ClockMath.cpp" "$root\src\SolarSchedule.cpp" "$root\src\ScheduleController.cpp" "$root\src\Config.cpp" "$root\src\ZipCodes.cpp" "$root\src\ZipLookup.cpp" "$root\src\ConfigManager.cpp" "$root\src\TimeService.cpp" "$root\src\LightingController.cpp" "$root\src\SetupRequest.cpp" "$root\src\SetupService.cpp" "$root\src\NetworkScan.cpp" "$root\src\SetupButton.cpp" "$root\src\StatusDisplay.cpp" "$root\src\MockDebug.cpp" /Fe:tests.exe
if %errorlevel% neq 0 exit /b %errorlevel%
tests.exe
if %errorlevel% neq 0 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /W4 /I"$root\tests\fakes" /I"$root\src" "$root\tests\test_devices.cpp" "$root\src\SensorManager.cpp" "$root\src\StatusLed.cpp" "$root\src\LightingController.cpp" "$root\src\TimeService.cpp" "$root\src\ClockMath.cpp" "$root\src\MockDebug.cpp" /Fe:devices-real.exe
if %errorlevel% neq 0 exit /b %errorlevel%
devices-real.exe
if %errorlevel% neq 0 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /W4 /DFISH_LAMP_BENCH=1 /DFISH_LAMP_CPU_TEMP=0 /DFISH_LAMP_STATUS_LED=0 /I"$root\tests\fakes" /I"$root\src" "$root\tests\test_devices.cpp" "$root\src\SensorManager.cpp" "$root\src\StatusLed.cpp" "$root\src\LightingController.cpp" "$root\src\TimeService.cpp" "$root\src\ClockMath.cpp" "$root\src\MockDebug.cpp" /Fe:devices-mock.exe
if %errorlevel% neq 0 exit /b %errorlevel%
devices-mock.exe
if %errorlevel% neq 0 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /W4 /I"$root\tests\fakes" /I"$root\src" /I"$ArduinoJson" "$root\tests\test_display.cpp" "$root\src\ZipLookup.cpp" "$root\src\ZipCodes.cpp" "$root\src\StatusDisplay.cpp" "$root\src\LightingController.cpp" "$root\src\TimeService.cpp" "$root\src\ClockMath.cpp" "$root\src\MockDebug.cpp" /Fe:display.exe
if %errorlevel% neq 0 exit /b %errorlevel%
display.exe
if %errorlevel% neq 0 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /W4 /DFISH_LAMP_OLED=0 /I"$root\tests\fakes" /I"$root\src" /I"$ArduinoJson" "$root\tests\test_display.cpp" "$root\src\ZipLookup.cpp" "$root\src\ZipCodes.cpp" "$root\src\StatusDisplay.cpp" "$root\src\LightingController.cpp" "$root\src\TimeService.cpp" "$root\src\ClockMath.cpp" "$root\src\MockDebug.cpp" /Fe:display-disabled.exe
if %errorlevel% neq 0 exit /b %errorlevel%
display-disabled.exe
exit /b %errorlevel%
"@ | Set-Content -LiteralPath $runner -Encoding ascii
Push-Location $out
try {
  & $env:ComSpec /d /c $runner
  if ($LASTEXITCODE -ne 0) { throw "Host tests failed with exit code $LASTEXITCODE" }
} finally { Pop-Location }
