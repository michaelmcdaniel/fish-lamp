$ErrorActionPreference = 'Stop'

$arduinoCliPath = & "$PSScriptRoot\tools\find-arduino-cli.ps1"

$projectDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDirectory = Join-Path $projectDirectory 'build'
& $arduinoCliPath compile `
    --fqbn 'rp2040:rp2040:rpipico2w:flash=4194304_262144' `
    --warnings all `
    --build-path $buildDirectory `
    $projectDirectory
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
