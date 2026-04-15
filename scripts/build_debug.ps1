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

function Get-EnvFromRegistry {
    param(
        [Parameter(Mandatory = $true)][string]$Name
    )

    $userValue = [Environment]::GetEnvironmentVariable($Name, "User")
    if (-not [string]::IsNullOrWhiteSpace($userValue)) {
        return $userValue
    }

    $machineValue = [Environment]::GetEnvironmentVariable($Name, "Machine")
    if (-not [string]::IsNullOrWhiteSpace($machineValue)) {
        return $machineValue
    }

    return $null
}

function Find-VulkanSdkPath {
    $candidates = @()

    $envSdk = Get-EnvFromRegistry -Name "VULKAN_SDK"
    if ($envSdk) {
        $candidates += $envSdk
    }

    $vkSdkPath = Get-EnvFromRegistry -Name "VK_SDK_PATH"
    if ($vkSdkPath) {
        $candidates += $vkSdkPath
    }

    if (Test-Path "C:/VulkanSDK") {
        $versionDirs = Get-ChildItem -Path "C:/VulkanSDK" -Directory -ErrorAction SilentlyContinue |
            Sort-Object -Property Name -Descending
        foreach ($dir in $versionDirs) {
            $candidates += $dir.FullName
        }
    }

    foreach ($path in $candidates | Select-Object -Unique) {
        if ([string]::IsNullOrWhiteSpace($path)) {
            continue
        }

        $normalized = $path.TrimEnd([char[]]@('\', '/'))
        $header = Join-Path $normalized "Include/vulkan/vulkan.h"
        $lib = Join-Path $normalized "Lib/vulkan-1.lib"
        if ((Test-Path $header) -and (Test-Path $lib)) {
            return $normalized
        }
    }

    return $null
}

function Ensure-VulkanEnvironment {
    $sdkPath = Find-VulkanSdkPath
    if (-not $sdkPath) {
        return $null
    }

    $env:VULKAN_SDK = $sdkPath
    if (-not $env:VK_SDK_PATH) {
        $env:VK_SDK_PATH = $sdkPath
    }

    $vulkanBin = Join-Path $sdkPath "Bin"
    if (Test-Path $vulkanBin) {
        if (-not (($env:Path -split ';') -contains $vulkanBin)) {
            $env:Path = "$vulkanBin;$env:Path"
        }
    }

    return $sdkPath
}

function Find-PkgConfigExecutable {
    $pkgConfig = Get-Command pkg-config -ErrorAction SilentlyContinue
    if ($pkgConfig) {
        return $pkgConfig.Source
    }

    $pkgconf = Get-Command pkgconf -ErrorAction SilentlyContinue
    if ($pkgconf) {
        return $pkgconf.Source
    }

    # Common MinGW installations provide pkgconf next to g++.
    $gcc = Get-Command g++ -ErrorAction SilentlyContinue
    if ($gcc) {
        $gccDir = Split-Path -Parent $gcc.Source
        $candidates = @(
            (Join-Path $gccDir "pkgconf.exe"),
            (Join-Path $gccDir "pkg-config.exe")
        )

        foreach ($candidate in $candidates) {
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }

    return $null
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

function Test-ClangToolchainAvailable {
    return ((Get-Command clang -ErrorAction SilentlyContinue) -and (Get-Command clang++ -ErrorAction SilentlyContinue))
}

function Test-MinGWToolchainAvailable {
    return [bool](Get-Command g++ -ErrorAction SilentlyContinue)
}

function Get-MinGWRoot {
    $gpp = Get-Command g++ -ErrorAction SilentlyContinue
    if (-not $gpp) {
        return $null
    }

    $binDir = Split-Path -Parent $gpp.Source
    if ([string]::IsNullOrWhiteSpace($binDir)) {
        return $null
    }

    $rootDir = Split-Path -Parent $binDir
    if ([string]::IsNullOrWhiteSpace($rootDir) -or -not (Test-Path $rootDir)) {
        return $null
    }

    return $rootDir
}

function Get-MinGWWindres {
    $candidates = @(
        (Get-Command windres -ErrorAction SilentlyContinue),
        (Get-Command x86_64-w64-mingw32-windres -ErrorAction SilentlyContinue)
    )

    foreach ($candidate in $candidates) {
        if ($candidate) {
            return $candidate.Source
        }
    }

    return $null
}

function Test-ClangWithMinGWLink {
    param(
        [Parameter(Mandatory = $true)][string]$MinGWRoot
    )

    $clang = Get-Command clang -ErrorAction SilentlyContinue
    if (-not $clang) {
        return $false
    }

    $probeDir = Join-Path ([System.IO.Path]::GetTempPath()) ("bess-clang-probe-" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $probeDir -Force | Out-Null

    try {
        $probeSource = Join-Path $probeDir "probe.c"
        $probeOutput = Join-Path $probeDir "probe.exe"
        Set-Content -Path $probeSource -Value "int main(void){return 0;}" -Encoding Ascii

        $probeArgs = @(
            "--target=x86_64-w64-windows-gnu"
            "--gcc-toolchain=$MinGWRoot"
            $probeSource
            "-o"
            $probeOutput
        )

        & $clang.Source @probeArgs *> $null
        return ($LASTEXITCODE -eq 0)
    }
    finally {
        Remove-Item $probeDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Get-PreferredBuildConfig {
    $hasNinja = [bool](Get-Command ninja -ErrorAction SilentlyContinue)
    $hasMake = ((Get-Command mingw32-make -ErrorAction SilentlyContinue) -or (Get-Command make -ErrorAction SilentlyContinue))
    $mingwRoot = Get-MinGWRoot

    if (Test-VisualStudioAvailable) {
        return @{
            Generator = "Visual Studio 17 2022"
            Toolchain = "MSVC"
            CCompiler = $null
            CxxCompiler = $null
            CCompilerTarget = $null
            CxxCompilerTarget = $null
            CExternalToolchain = $null
            CxxExternalToolchain = $null
            RcCompiler = $null
        }
    }

    if (Test-ClangToolchainAvailable) {
        $generator = if ($hasNinja) { "Ninja" } elseif ($hasMake) { "MinGW Makefiles" } else { $null }
        if ($generator -and $mingwRoot -and (Test-ClangWithMinGWLink -MinGWRoot $mingwRoot)) {
            return @{
                Generator = $generator
                Toolchain = "Clang"
                CCompiler = "clang"
                CxxCompiler = "clang++"
                CCompilerTarget = "x86_64-w64-windows-gnu"
                CxxCompilerTarget = "x86_64-w64-windows-gnu"
                CExternalToolchain = $mingwRoot
                CxxExternalToolchain = $mingwRoot
                RcCompiler = Get-MinGWWindres
            }
        }

        Write-Warning "Clang was found but cannot link with available Windows libraries/toolchain. Falling back to MinGW."
    }

    if (Test-MinGWToolchainAvailable) {
        $generator = if ($hasNinja) { "Ninja" } elseif ($hasMake) { "MinGW Makefiles" } else { $null }
        if ($generator) {
            return @{
                Generator = $generator
                Toolchain = "MinGW"
                CCompiler = "gcc"
                CxxCompiler = "g++"
                CCompilerTarget = $null
                CxxCompilerTarget = $null
                CExternalToolchain = $null
                CxxExternalToolchain = $null
                RcCompiler = Get-MinGWWindres
            }
        }
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

function Get-CachedCompiler {
    param(
        [Parameter(Mandatory = $true)][string]$VarName
    )

    $cacheFile = Join-Path $buildDir "CMakeCache.txt"
    if (-not (Test-Path $cacheFile)) {
        return $null
    }

    $line = Select-String -Path $cacheFile -Pattern "^${VarName}:FILEPATH=" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $line) {
        return $null
    }

    return ($line.Line -replace "^${VarName}:FILEPATH=", '').Trim()
}

function Get-CachedValue {
    param(
        [Parameter(Mandatory = $true)][string]$VarName
    )

    $cacheFile = Join-Path $buildDir "CMakeCache.txt"
    if (-not (Test-Path $cacheFile)) {
        return $null
    }

    $line = Select-String -Path $cacheFile -Pattern "^${VarName}:[^=]+=" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $line) {
        return $null
    }

    return ($line.Line -replace "^${VarName}:[^=]+=", '').Trim()
}

function Normalize-CompilerName {
    param(
        [string]$Name
    )

    if ([string]::IsNullOrWhiteSpace($Name)) {
        return ""
    }

    $leaf = Split-Path $Name -Leaf
    if ([string]::IsNullOrWhiteSpace($leaf)) {
        $leaf = $Name
    }

    return $leaf.ToLowerInvariant()
}

function Normalize-PathString {
    param(
        [string]$PathValue
    )

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        return ""
    }

    return (($PathValue -replace '\\', '/').TrimEnd('/')).ToLowerInvariant()
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
$vulkanSdkPath = Ensure-VulkanEnvironment
$buildConfig = Get-PreferredBuildConfig

if (-not $buildConfig) {
    throw "No supported build toolchain found. Install one of: Visual Studio 2022 Build Tools, Clang (clang/clang++ plus Ninja), or MinGW-w64 (g++ plus Ninja or mingw32-make)."
}

$generator = $buildConfig.Generator
$toolchain = $buildConfig.Toolchain
$configureArgs = @("-Wno-dev", "-Wno-deprecated", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DCMAKE_BUILD_TYPE=Debug")
if ($buildConfig.CCompiler) {
    $configureArgs += "-DCMAKE_C_COMPILER=$($buildConfig.CCompiler)"
}
if ($buildConfig.CxxCompiler) {
    $configureArgs += "-DCMAKE_CXX_COMPILER=$($buildConfig.CxxCompiler)"
}
if ($buildConfig.CCompilerTarget) {
    $configureArgs += "-DCMAKE_C_COMPILER_TARGET=$($buildConfig.CCompilerTarget)"
}
if ($buildConfig.CxxCompilerTarget) {
    $configureArgs += "-DCMAKE_CXX_COMPILER_TARGET=$($buildConfig.CxxCompilerTarget)"
}
if ($buildConfig.CExternalToolchain) {
    $configureArgs += "-DCMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN=$($buildConfig.CExternalToolchain)"
}
if ($buildConfig.CxxExternalToolchain) {
    $configureArgs += "-DCMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN=$($buildConfig.CxxExternalToolchain)"
}
if ($buildConfig.RcCompiler) {
    $configureArgs += "-DCMAKE_RC_COMPILER=$($buildConfig.RcCompiler)"
}

$pkgConfigExecutable = Find-PkgConfigExecutable
if ($pkgConfigExecutable) {
    $configureArgs += "-DPKG_CONFIG_EXECUTABLE=$pkgConfigExecutable"
    Write-Host "Using pkg-config executable: $pkgConfigExecutable"
} else {
    $configureArgs += "-DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=ON"
    Write-Host "pkg-config executable not found; disabling PkgConfig package discovery."
}

if ($vulkanSdkPath) {
    $configureArgs += "-DVulkan_ROOT=$vulkanSdkPath"
    Write-Host "Using Vulkan SDK: $vulkanSdkPath"
} else {
    Write-Warning "Vulkan SDK location not detected in current shell. If configuration fails with Vulkan not found, restart PowerShell or reinstall LunarG SDK."
}

Write-Host "Using toolchain: $toolchain"
Write-Host "Using CMake generator: $generator"
if ($buildConfig.CExternalToolchain) {
    Write-Host "Using external toolchain root: $($buildConfig.CExternalToolchain)"
}

$cachedGenerator = Get-CachedGenerator
$cachedCCompiler = Get-CachedCompiler -VarName "CMAKE_C_COMPILER"
$cachedCxxCompiler = Get-CachedCompiler -VarName "CMAKE_CXX_COMPILER"
$cachedCCompilerTarget = Get-CachedValue -VarName "CMAKE_C_COMPILER_TARGET"
$cachedCxxCompilerTarget = Get-CachedValue -VarName "CMAKE_CXX_COMPILER_TARGET"
$cachedCExternalToolchain = Get-CachedValue -VarName "CMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN"
$cachedCxxExternalToolchain = Get-CachedValue -VarName "CMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN"

$compilerMismatch = $false
if ($buildConfig.CCompiler) {
    $compilerMismatch = $compilerMismatch -or (Normalize-CompilerName $cachedCCompiler) -ne (Normalize-CompilerName $buildConfig.CCompiler)
}
if ($buildConfig.CxxCompiler) {
    $compilerMismatch = $compilerMismatch -or (Normalize-CompilerName $cachedCxxCompiler) -ne (Normalize-CompilerName $buildConfig.CxxCompiler)
}
if ($buildConfig.CCompilerTarget) {
    $cachedCTarget = ""
    if ($cachedCCompilerTarget) {
        $cachedCTarget = $cachedCCompilerTarget.ToLowerInvariant()
    }
    $compilerMismatch = $compilerMismatch -or ($cachedCTarget -ne $buildConfig.CCompilerTarget.ToLowerInvariant())
}
if ($buildConfig.CxxCompilerTarget) {
    $cachedCxxTarget = ""
    if ($cachedCxxCompilerTarget) {
        $cachedCxxTarget = $cachedCxxCompilerTarget.ToLowerInvariant()
    }
    $compilerMismatch = $compilerMismatch -or ($cachedCxxTarget -ne $buildConfig.CxxCompilerTarget.ToLowerInvariant())
}
if ($buildConfig.CExternalToolchain) {
    $compilerMismatch = $compilerMismatch -or (Normalize-PathString $cachedCExternalToolchain) -ne (Normalize-PathString $buildConfig.CExternalToolchain)
}
if ($buildConfig.CxxExternalToolchain) {
    $compilerMismatch = $compilerMismatch -or (Normalize-PathString $cachedCxxExternalToolchain) -ne (Normalize-PathString $buildConfig.CxxExternalToolchain)
}

if (($cachedGenerator -and $cachedGenerator -ne $generator) -or $compilerMismatch) {
    Write-Host "Detected incompatible cached CMake configuration. Resetting cache..."
    Reset-BuildCache
}

if (Test-Path (Join-Path $buildDir "CMakeCache.txt")) {
    cmake -S $rootDir -B $buildDir @configureArgs
} elseif ($generator -like "Visual Studio*") {
    cmake -S $rootDir -B $buildDir -G $generator -A x64 @configureArgs
} else {
    cmake -S $rootDir -B $buildDir -G $generator @configureArgs
}
cmake --build $buildDir --config Debug --parallel

$compileCommands = Join-Path $buildDir "compile_commands.json"
if (Test-Path $compileCommands) {
    Copy-Item $compileCommands (Join-Path $rootDir "compile_commands.json") -Force
}
