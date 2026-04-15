$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Resolve-Path (Join-Path $scriptDir "..")

& (Join-Path $scriptDir "bootstrap_deps.ps1")

$generator = "Visual Studio 17 2022"
if (Get-Command ninja -ErrorAction SilentlyContinue) {
    $generator = "Ninja"
}

if (Test-Path (Join-Path $rootDir "build/CMakeCache.txt")) {
    cmake -S $rootDir -B (Join-Path $rootDir "build") -DCMAKE_BUILD_TYPE=Release
} elseif ($generator -like "Visual Studio*") {
    cmake -S $rootDir -B (Join-Path $rootDir "build") -G $generator -A x64 -DCMAKE_BUILD_TYPE=Release
} else {
    cmake -S $rootDir -B (Join-Path $rootDir "build") -G $generator -DCMAKE_BUILD_TYPE=Release
}
cmake --build (Join-Path $rootDir "build") --config Release --parallel
