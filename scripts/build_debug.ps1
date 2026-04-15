$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Resolve-Path (Join-Path $scriptDir "..")
$buildDir = Join-Path $rootDir "build"

& (Join-Path $scriptDir "bootstrap_deps.ps1")

function Refresh-PathFromRegistry {
    # winget updates PATH for future shells; refresh current session so tools are discoverable now.
    try {
        $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
        $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
        $combined = @($machinePath, $userPath) -join ";"
        if (-not [string]::IsNullOrWhiteSpace($combined)) {
            $env:Path = $combined
        }
    } catch {
        Write-Verbose "Unable to refresh PATH from registry: $_"
    }
}

function Test-VisualStudioAvailable {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
    if (Test-Path $vswhere) {
        $installation = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath 2>$null
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($installation)) {
            return $true
        }
    }

    # Fallback heuristic when vswhere is unavailable.
    return [bool](Get-Command cl -ErrorAction SilentlyContinue)
}

function Get-PreferredGenerator {
    if (Get-Command ninja -ErrorAction SilentlyContinue) {
        return "Ninja"
    }

    if (Test-VisualStudioAvailable) {
        return "Visual Studio 17 2022"
    }

    if ((Get-Command g++ -ErrorAction SilentlyContinue) -and
        ((Get-Command mingw32-make -ErrorAction SilentlyContinue) -or (Get-Command make -ErrorAction SilentlyContinue))) {
        return "MinGW Makefiles"
    }

    return $null
}

function Get-CachedGenerator {
    $cacheFile = Join-Path $buildDir "CMakeCache.txt"
    if (-not (Test-Path $cacheFile)) {
        return $null
    }

    $line = Select-String -Path $cacheFile -Pattern '^CMAKE_GENERATOR:INTERNAL=' -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $line) {
        return $null
    }

    return ($line.Line -replace '^CMAKE_GENERATOR:INTERNAL=', '').Trim()
}

function Reset-BuildCache {
    $cacheFile = Join-Path $buildDir "CMakeCache.txt"
    $cmakeFilesDir = Join-Path $buildDir "CMakeFiles"

    if (Test-Path $cacheFile) {
        Remove-Item $cacheFile -Force
    }

    if (Test-Path $cmakeFilesDir) {
        Remove-Item $cmakeFilesDir -Recurse -Force
    }
}

Refresh-PathFromRegistry
$generator = Get-PreferredGenerator

if (-not $generator) {
    throw "No supported build generator found. Install one of: Ninja, Visual Studio 2022 Build Tools, or MinGW-w64 (g++ and mingw32-make)."
}

Write-Host "Using CMake generator: $generator"

$cachedGenerator = Get-CachedGenerator
if ($cachedGenerator -and $cachedGenerator -ne $generator) {
    Write-Host "Detected cached CMake generator '$cachedGenerator' which differs from '$generator'. Resetting cache..."
    Reset-BuildCache
}

if (Test-Path (Join-Path $buildDir "CMakeCache.txt")) {
    cmake -S $rootDir -B $buildDir -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug
} elseif ($generator -like "Visual Studio*") {
    cmake -S $rootDir -B $buildDir -G $generator -A x64 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug
} else {
    cmake -S $rootDir -B $buildDir -G $generator -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug
}
cmake --build $buildDir --config Debug --parallel

$compileCommands = Join-Path $buildDir "compile_commands.json"
if (Test-Path $compileCommands) {
    Copy-Item $compileCommands (Join-Path $rootDir "compile_commands.json") -Force
}
