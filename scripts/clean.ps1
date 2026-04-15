$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Resolve-Path (Join-Path $scriptDir "..")

$paths = @(
    (Join-Path $rootDir "build"),
    (Join-Path $rootDir "bin"),
    (Join-Path $rootDir "compile_commands.json")
)

foreach ($path in $paths) {
    Remove-Item -Path $path -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "Clean complete"
