$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$directory = Join-Path $root 'test-build'
$fixture = Join-Path $directory 'migration-input.json'
$output = Join-Path $directory ('migration-' + [guid]::NewGuid().ToString() + '.json')
$badOutput = Join-Path $directory ('rejected-' + [guid]::NewGuid().ToString() + '.json')
$source = @{
  lights = @(1..85 | ForEach-Object { ,@(16,32,64,128) })
  nightlights = @(1..85 | ForEach-Object { ,@(0,0,8,16) })
  'lights-sunrise-offset' = 0; 'lights-sunset-offset' = 0; 'lights-max-offset' = 86400
  'growlight-sunrise-offset' = 0; 'growlight-sunset-offset' = 0; 'growlight-max-offset' = 86400
  createDate = 'synthetic-test'; modDate = 'synthetic-test'
}
$utf8 = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($fixture, ($source | ConvertTo-Json -Depth 8 -Compress), $utf8)
$before = (Get-FileHash -LiteralPath $fixture).Hash
& "$root\tools\migrate-config.ps1" -InputPath $fixture -OutputPath $output -Latitude 0 -Longitude 0 -TimezoneOffsetMinutes 0 -Dst none -RtcUtcConfirmed
if ((Get-FileHash -LiteralPath $fixture).Hash -ne $before) { throw 'Source changed' }
$actual = Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
if ($actual.lights.Count -ne 85 -or $actual.lights[84][3] -ne 128 -or $actual.createDate -ne 'synthetic-test') { throw 'Migration lost fields' }
$hash = (Get-FileHash -LiteralPath $output).Hash
$rejected = $false
try {
  & "$root\tools\migrate-config.ps1" -InputPath $fixture -OutputPath $output -Latitude 0 -Longitude 0 -TimezoneOffsetMinutes 0 -Dst none -RtcUtcConfirmed
} catch { $rejected = $true }
if (-not $rejected -or (Get-FileHash -LiteralPath $output).Hash -ne $hash) { throw 'Overwrite was not rejected' }
$source.lights[84][3] = 256
[System.IO.File]::WriteAllText($fixture, ($source | ConvertTo-Json -Depth 8 -Compress), $utf8)
$rejected = $false
try {
  & "$root\tools\migrate-config.ps1" -InputPath $fixture -OutputPath $badOutput -Latitude 0 -Longitude 0 -TimezoneOffsetMinutes 0 -Dst none -RtcUtcConfirmed
} catch { $rejected = $true }
if (-not $rejected -or (Test-Path -LiteralPath $badOutput)) { throw 'Invalid config was not rejected' }
Write-Output 'Migration checks passed: values retained, source unchanged, overwrite and invalid channels rejected.'
