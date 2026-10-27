param([string]$U8g2Path)
$ErrorActionPreference = 'Stop'

$arduinoCliPath = & "$PSScriptRoot\tools\find-arduino-cli.ps1"

$projectDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDirectory = Join-Path $projectDirectory 'build/pico2w'
# Prefer the clean local package over OneDrive library placeholders.
if (-not $U8g2Path) {
    $localU8g2Path = Join-Path $projectDirectory 'build/dependencies/U8g2-2.36.19'
    if (Test-Path -LiteralPath (Join-Path $localU8g2Path 'library.properties')) {
        $U8g2Path = $localU8g2Path
    }
}
$libraryArguments = @()
if ($U8g2Path) { $libraryArguments = @('--library', (Resolve-Path -LiteralPath $U8g2Path).Path) }
& $arduinoCliPath compile `
    --fqbn 'rp2040:rp2040:rpipico2w:flash=4194304_262144' `
    --warnings all `
    --build-path $buildDirectory `
    @libraryArguments `
    $projectDirectory
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
