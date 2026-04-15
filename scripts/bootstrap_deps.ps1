$ErrorActionPreference = "Stop"

if ($env:BESS_SKIP_BOOTSTRAP -eq "1") {
    Write-Host "Skipping dependency bootstrap because BESS_SKIP_BOOTSTRAP=1"
    exit 0
}

if ($env:BESS_AUTO_BOOTSTRAP -eq "1") {
    $script:BessBootstrapConfirmed = $true
} else {
    $script:BessBootstrapConfirmed = $false
}

function Confirm-Bootstrap {
    if ($script:BessBootstrapConfirmed) {
        return $true
    }

    if (-not [Environment]::UserInteractive) {
        Write-Warning "Skipping dependency bootstrap (non-interactive shell). Set BESS_AUTO_BOOTSTRAP=1 to allow automatic install."
        return $false
    }

    $answer = Read-Host "Dependency bootstrap may install system packages. Continue? [y/N]"
    if ($answer -match '^(?i:y|yes)$') {
        $script:BessBootstrapConfirmed = $true
        return $true
    }

    Write-Host "Skipping dependency bootstrap by user choice."
    return $false
}

function Ensure-Winget {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        throw "winget is required for automatic dependency bootstrap on Windows."
    }
}

function Ensure-Package {
    param(
        [Parameter(Mandatory = $true)][string]$Id,
        [Parameter(Mandatory = $true)][string]$Name,
        [switch]$Optional
    )

    $found = winget list --id $Id --exact 2>$null
    if ($LASTEXITCODE -eq 0 -and $found) {
        Write-Host "$Name already installed"
        return
    }

    Write-Host "Installing $Name..."
    winget install --id $Id --exact --accept-source-agreements --accept-package-agreements --silent
    if ($LASTEXITCODE -ne 0) {
        if ($Optional) {
            Write-Warning "Failed to install optional package '$Name' (id: $Id)."
            return
        }

        throw "Failed to install required package '$Name' (id: $Id)."
    }
}

function Ensure-FirstAvailablePackage {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string[]]$Ids
    )

    foreach ($id in $Ids) {
        $found = winget list --id $id --exact 2>$null
        if ($LASTEXITCODE -eq 0 -and $found) {
            Write-Host "$Name already installed"
            return
        }
    }

    foreach ($id in $Ids) {
        Write-Host "Installing $Name..."
        winget install --id $id --exact --accept-source-agreements --accept-package-agreements --silent
        if ($LASTEXITCODE -eq 0) {
            return
        }
    }

    Write-Warning "Could not install optional package '$Name'. Tried ids: $($Ids -join ', ')."
}

function Get-PythonCommand {
    if (Get-Command py -ErrorAction SilentlyContinue) {
        return @("py", "-3")
    }

    if (Get-Command python -ErrorAction SilentlyContinue) {
        return @("python")
    }

    return $null
}

function Ensure-PythonPackage {
    param(
        [Parameter(Mandatory = $true)][string]$PackageName,
        [string]$ImportName = $PackageName
    )

    $pythonCmd = Get-PythonCommand
    if (-not $pythonCmd) {
        Write-Warning "Python was not found on PATH; skipping Python package '$PackageName'."
        return
    }

    $pythonExe = $pythonCmd[0]
    $pythonArgs = @()
    if ($pythonCmd.Count -gt 1) {
        $pythonArgs = $pythonCmd[1..($pythonCmd.Count - 1)]
    }

    # Use an import probe instead of `pip show` to avoid stderr warnings being promoted to terminating errors.
    $checkCode = "import importlib.util,sys; sys.exit(0 if importlib.util.find_spec('$ImportName') else 1)"
    & $pythonExe @pythonArgs -c $checkCode 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "$PackageName already installed"
        return
    }

    Write-Host "Installing Python package $PackageName..."
    & $pythonExe @pythonArgs -m pip install --user $PackageName
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Failed to install Python package '$PackageName'."
    }
}

if (-not (Confirm-Bootstrap)) {
    Write-Host "Dependency bootstrap finished."
    exit 0
}

Ensure-Winget
Ensure-Package -Id "Git.Git" -Name "Git"
Ensure-Package -Id "Kitware.CMake" -Name "CMake"
Ensure-Package -Id "Ninja-build.Ninja" -Name "Ninja"
Ensure-FirstAvailablePackage -Name "Vulkan SDK" -Ids @("LunarG.VulkanSDK", "KhronosGroup.VulkanSDK")
Ensure-PythonPackage -PackageName "pybind11" -ImportName "pybind11"
Ensure-PythonPackage -PackageName "pybind11-stubgen" -ImportName "pybind11_stubgen"

Write-Host "Dependency bootstrap finished."
