$ErrorActionPreference = 'Stop'

# User-local install: no administrator rights or persistent PATH changes needed.
$command = Get-Command arduino-cli -ErrorAction SilentlyContinue
$cli = if ($command) { $command.Source } else { "$env:ProgramFiles\Arduino CLI\arduino-cli.exe" }
if (-not (Test-Path -LiteralPath $cli)) {
    $cli = "$env:LOCALAPPDATA\Programs\Arduino CLI\arduino-cli.exe"
}
if (-not (Test-Path -LiteralPath $cli)) {
    $version = '1.5.1'
    $archiveName = "arduino-cli_${version}_Windows_64bit.zip"
    $releaseUrl = "https://github.com/arduino/arduino-cli/releases/download/v$version"
    $staging = Join-Path ([IO.Path]::GetTempPath()) ("fish-lamp-tools-" + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $staging | Out-Null
    $archive = Join-Path $staging $archiveName
    Write-Host "Downloading Arduino CLI $version from the official release..."
    Invoke-WebRequest -UseBasicParsing -Uri "$releaseUrl/$archiveName" -OutFile $archive
    $checksumFile = Join-Path $staging 'checksums.txt'
    Invoke-WebRequest -UseBasicParsing -Uri "$releaseUrl/$version-checksums.txt" -OutFile $checksumFile
    $line = Get-Content -LiteralPath $checksumFile | Where-Object { $_.Trim().EndsWith(" $archiveName") }
    if (@($line).Count -ne 1) { throw 'Official archive checksum was not found.' }
    $expected = ($line.Trim() -split '\s+')[0]
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) {
        throw 'Arduino CLI archive checksum does not match.'
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $staging
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $cli) | Out-Null
    Copy-Item -LiteralPath (Join-Path $staging 'arduino-cli.exe') -Destination $cli
}

function Invoke-Arduino {
    & $cli @args
    if ($LASTEXITCODE -ne 0) { throw "Arduino CLI failed: $args" }
}

Invoke-Arduino version
$urlsJson = & $cli config get board_manager.additional_urls --json
if ($LASTEXITCODE -ne 0) { throw 'Could not read Arduino CLI configuration.' }
$urls = $urlsJson | ConvertFrom-Json
$index = 'https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json'
if ($urls -notcontains $index) {
    Invoke-Arduino config add board_manager.additional_urls $index
}
Invoke-Arduino core update-index
Invoke-Arduino core install 'rp2040:rp2040@6.0.0'
Invoke-Arduino lib install 'ArduinoJson@7.4.3' 'Adafruit NeoPixel@1.15.2' 'OneWire@2.3.8'
Write-Host 'Build tools ready. Run ./build.ps1.'
