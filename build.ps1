<#
.SYNOPSIS
    Build script for nexus-log-uploader.
.EXAMPLE
    .\build.ps1                     # Release x64 (default)
    .\build.ps1 -Config Debug       # Debug x64
    .\build.ps1 -Clean              # Clean build artifacts
    .\build.ps1 -Rebuild            # Full rebuild
    .\build.ps1 -Config Debug -Mock # Build + launch in mock
#>
[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",

    [switch]$Clean,
    [switch]$Rebuild,
    [switch]$Mock
)

$ErrorActionPreference = "Stop"

$VsWherePath = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $VsWherePath)) {
    Write-Error "vswhere not found. Install Visual Studio or Build Tools."
    exit 1
}

$MSBuildPath = & $VsWherePath -latest -products * -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $MSBuildPath) {
    Write-Error "MSBuild not found. Install Visual Studio or Build Tools."
    exit 1
}

function Invoke-MSBuild {
    param([string]$Solution, [string[]]$ExtraArgs)
    $InvokeArgs = @($Solution, "/p:Configuration=$Config", "/p:Platform=x64", "/m", "/v:minimal") + $ExtraArgs
    & $MSBuildPath @InvokeArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$SolutionPath = Join-Path $PSScriptRoot "nexus_log_uploader.sln"

Write-Host "[$Config] " -NoNewline -ForegroundColor Cyan
if ($Clean) {
    Write-Host "Cleaning..."
    Invoke-MSBuild -Solution $SolutionPath -ExtraArgs "/t:Clean"
}
elseif ($Rebuild) {
    Write-Host "Rebuilding..."
    Invoke-MSBuild -Solution $SolutionPath -ExtraArgs "/t:Rebuild"
}
else {
    Write-Host "Building..."
    Invoke-MSBuild -Solution $SolutionPath
}

if ($Mock) {
    $MockDir = Join-Path $PSScriptRoot "mock"
    $MockSolution = Join-Path $MockDir "nexus_mock.sln"
    if (-not (Test-Path $MockSolution)) {
        Write-Error "Mock not found at $MockDir. Run: git submodule update --init mock"
        exit 1
    }

    Write-Host "[$Config] " -NoNewline -ForegroundColor Cyan
    Write-Host "Building mock..."
    Invoke-MSBuild -Solution $MockSolution

    $MockExe = Join-Path $MockDir "build\x64\$Config\nexus_mock.exe"
    $AddonDll = Join-Path $PSScriptRoot "build\x64\$Config\log_uploader.dll"

    if (-not (Test-Path $MockExe)) {
        Write-Error "Mock executable not found: $MockExe"
        exit 1
    }
    if (-not (Test-Path $AddonDll)) {
        Write-Error "Addon DLL not found: $AddonDll"
        exit 1
    }

    Write-Host "Launching mock..." -ForegroundColor Green
    & $MockExe $AddonDll
}
