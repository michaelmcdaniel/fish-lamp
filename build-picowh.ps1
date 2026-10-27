$ErrorActionPreference = 'Stop'

$arduinoCliPath = & "$PSScriptRoot\tools\find-arduino-cli.ps1"

$projectDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDirectory = Join-Path $projectDirectory 'build/picowh'
& $arduinoCliPath compile `
    --fqbn 'rp2040:rp2040:rpipicow:flash=2097152_262144' `
    --warnings all `
    --build-path $buildDirectory `
    $projectDirectory
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
