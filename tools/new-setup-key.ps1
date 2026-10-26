$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$header = Join-Path $root 'src\SetupSecrets.h'
$instructions = Join-Path $root 'setup-access.txt'
if ((Test-Path -LiteralPath $header) -or (Test-Path -LiteralPath $instructions)) {
  throw 'Setup access files already exist. Keep them for subsequent builds; this script never replaces them.'
}
$bytes = New-Object byte[] 16
$rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
try { $rng.GetBytes($bytes) } finally { $rng.Dispose() }
$password = [Convert]::ToBase64String($bytes)
$utf8 = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($header, "#pragma once`n#define FISH_LAMP_SETUP_PASSWORD `"$password`"`n", $utf8)
[System.IO.File]::WriteAllText($instructions, "Network: FishLamp-Setup`r`nPassword: $password`r`nPage: http://192.168.4.1/`r`nKeep this file private. These access files are excluded from Git.`r`n", $utf8)
Write-Output 'Created private setup-access.txt and src/SetupSecrets.h. Password was not printed.'
