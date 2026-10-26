param(
  [Parameter(Mandatory=$true)][string]$InputPath,
  [Parameter(Mandatory=$true)][string]$OutputPath,
  [Parameter(Mandatory=$true)][ValidateRange(-90,90)][double]$Latitude,
  [Parameter(Mandatory=$true)][ValidateRange(-180,180)][double]$Longitude,
  [Parameter(Mandatory=$true)][ValidateRange(-720,840)][int]$TimezoneOffsetMinutes,
  [Parameter(Mandatory=$true)][ValidateSet('none','us')][string]$Dst,
  [switch]$RtcUtcConfirmed
)
$ErrorActionPreference = 'Stop'
if (-not $RtcUtcConfirmed) { throw 'Verify the DS3231 stores UTC before migration; then supply -RtcUtcConfirmed.' }
if (Test-Path -LiteralPath $OutputPath) { throw 'Output already exists; migration never overwrites a file.' }
$validator = Join-Path (Split-Path $PSScriptRoot -Parent) 'test-build\tests.exe'
if (-not (Test-Path -LiteralPath $validator)) { throw 'Run test.ps1 first to build the configuration validator.' }
$source = Get-Content -LiteralPath $InputPath -Raw | ConvertFrom-Json
$source | Add-Member -Force -NotePropertyName 'schema-version' -NotePropertyValue 1
$source | Add-Member -Force -NotePropertyName 'rtc-utc-confirmed' -NotePropertyValue $true
$source | Add-Member -Force -NotePropertyName 'latitude' -NotePropertyValue $Latitude
$source | Add-Member -Force -NotePropertyName 'longitude' -NotePropertyValue $Longitude
$source | Add-Member -Force -NotePropertyName 'timezone-offset-minutes' -NotePropertyValue $TimezoneOffsetMinutes
$source | Add-Member -Force -NotePropertyName 'timezone-dst' -NotePropertyValue $Dst
$json = $source | ConvertTo-Json -Depth 8 -Compress
$bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($json)
# Validate inside ignored test-build first; private content is never printed.
$temporary = Join-Path (Split-Path $validator -Parent) ([guid]::NewGuid().ToString() + '.json')
try {
  [System.IO.File]::WriteAllBytes($temporary, $bytes)
  & $validator --validate $temporary
  if ($LASTEXITCODE -ne 0) { throw 'Migration rejected: original settings need correction; no output written.' }
  $stream = [System.IO.File]::Open($OutputPath, [System.IO.FileMode]::CreateNew)
  try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
} finally {
  if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
}
