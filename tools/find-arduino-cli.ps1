$command = Get-Command arduino-cli -ErrorAction SilentlyContinue
if ($command) { return $command.Source }
foreach ($candidate in @(
    "$env:LOCALAPPDATA\Programs\Arduino CLI\arduino-cli.exe",
    "$env:ProgramFiles\Arduino CLI\arduino-cli.exe"
)) {
    if (Test-Path -LiteralPath $candidate) { return $candidate }
}
throw 'arduino-cli was not found. Run ./setup-tools.ps1 first.'
