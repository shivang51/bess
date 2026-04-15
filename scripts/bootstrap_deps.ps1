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

if (-not (Confirm-Bootstrap)) {
    Write-Host "Dependency bootstrap finished."
    exit 0
}

Ensure-Winget
Ensure-Package -Id "Git.Git" -Name "Git"
Ensure-Package -Id "Kitware.CMake" -Name "CMake"
Ensure-Package -Id "Ninja-build.Ninja" -Name "Ninja"
Ensure-FirstAvailablePackage -Name "Vulkan SDK" -Ids @("LunarG.VulkanSDK", "KhronosGroup.VulkanSDK")

Write-Host "Dependency bootstrap finished."
