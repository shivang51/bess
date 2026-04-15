$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Resolve-Path (Join-Path $scriptDir "..")

& (Join-Path $scriptDir "build_debug.ps1")

$exePath = Join-Path $rootDir "bin/Debug/x64/Bess.exe"
if (-not (Test-Path $exePath)) {
    throw "Unable to find debug executable at $exePath"
}

& $exePath @args
