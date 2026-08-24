[CmdletBinding()]
param(
    [string]$Destination
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$projectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Join-Path $projectRoot 'build\dependencies\wms-rc4'
}

$packages = @(
    @{
        Name = 'Microsoft.Windows.Devices.Midi2'
        Version = '1.0.17-rc.4.25'
        Uri = 'https://github.com/microsoft/MIDI/releases/download/rc-4/Microsoft.Windows.Devices.Midi2.1.0.17-rc.4.25.nupkg'
        FileName = 'Microsoft.Windows.Devices.Midi2.1.0.17-rc.4.25.nupkg'
        Sha256 = 'D0A420E724154AAF707CBCEEFDE0E355B0B6D9DCD37160F767BFAC6A9C9A86E6'
        ExtractDirectory = 'package'
        RequiredFile = 'ref\native\Microsoft.Windows.Devices.Midi2.winmd'
    },
    @{
        Name = 'Microsoft.Windows.CppWinRT'
        Version = '2.0.240405.15'
        Uri = 'https://api.nuget.org/v3-flatcontainer/microsoft.windows.cppwinrt/2.0.240405.15/microsoft.windows.cppwinrt.2.0.240405.15.nupkg'
        FileName = 'Microsoft.Windows.CppWinRT.2.0.240405.15.nupkg'
        Sha256 = 'E889007B5D9235931E7340DDF737D2C346EEBDD23C619F1F4F2426A2AAE47180'
        ExtractDirectory = 'cppwinrt-package'
        RequiredFile = 'bin\cppwinrt.exe'
    }
)

New-Item -ItemType Directory -Force -Path $Destination | Out-Null

foreach ($package in $packages) {
    $archivePath = Join-Path $Destination $package.FileName
    if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
        Write-Host "Downloading $($package.Name) $($package.Version)"
        Invoke-WebRequest -Uri $package.Uri -OutFile $archivePath
    }

    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archivePath).Hash
    if ($actualHash -ne $package.Sha256) {
        throw "SHA-256 mismatch for $archivePath. Expected $($package.Sha256), got $actualHash."
    }

    $extractPath = Join-Path $Destination $package.ExtractDirectory
    $requiredPath = Join-Path $extractPath $package.RequiredFile
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        if (Test-Path -LiteralPath $extractPath) {
            throw "Incomplete extraction exists at $extractPath. Remove that project-local directory and retry."
        }
        Expand-Archive -LiteralPath $archivePath -DestinationPath $extractPath
    }

    Write-Host "$($package.Name) $($package.Version): verified $actualHash"
}

$sdkWinmd = Join-Path $Destination 'package\ref\native\Microsoft.Windows.Devices.Midi2.winmd'
$sdkWinmdHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sdkWinmd).Hash
if ($sdkWinmdHash -ne 'EC63F2C944ECD678A88D73A9D2BFF736606558E5FAAB6AEB3FF287DE199FF750') {
    throw "Unexpected WMS SDK WINMD hash: $sdkWinmdHash"
}

Write-Host "WMS SDK WINMD: verified $sdkWinmdHash"
Write-Host "Dependencies ready at $Destination"
