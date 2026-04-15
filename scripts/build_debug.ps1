$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Resolve-Path (Join-Path $scriptDir "..")

& (Join-Path $scriptDir "bootstrap_deps.ps1")

$generator = "Visual Studio 17 2022"
if (Get-Command ninja -ErrorAction SilentlyContinue) {
    $generator = "Ninja"
}

if (Test-Path (Join-Path $rootDir "build/CMakeCache.txt")) {
    cmake -S $rootDir -B (Join-Path $rootDir "build") -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug
} elseif ($generator -like "Visual Studio*") {
    cmake -S $rootDir -B (Join-Path $rootDir "build") -G $generator -A x64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug
} else {
    cmake -S $rootDir -B (Join-Path $rootDir "build") -G $generator -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug
}
cmake --build (Join-Path $rootDir "build") --config Debug --parallel

$compileCommands = Join-Path $rootDir "build/compile_commands.json"
if (Test-Path $compileCommands) {
    Copy-Item $compileCommands (Join-Path $rootDir "compile_commands.json") -Force
}
