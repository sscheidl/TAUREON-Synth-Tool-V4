[CmdletBinding()]
param(
    [string]$Destination
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$projectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Join-Path $projectRoot 'build\dependencies\wms-preview9'
}

$packages = @(
    @{
        Name = 'Windows.Devices.Midi2'
        Version = '0.99.83-devpreview.9'
        Uri = 'https://github.com/microsoft/MIDI/releases/download/inbox-dev-preview-9/Windows.Devices.Midi2.0.99.83-devpreview.9.nupkg'
        FileName = 'Windows.Devices.Midi2.0.99.83-devpreview.9.nupkg'
        Sha256 = 'CCB4D0A4358D16F7ECDBFEE0FF6B08278E4951B9D9C3A103478DE6F5E7CA99E8'
        ExtractDirectory = 'package'
        RequiredFile = 'ref\native\Windows.Devices.Midi2.winmd'
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

$sdkWinmd = Join-Path $Destination 'package\ref\native\Windows.Devices.Midi2.winmd'
$sdkWinmdHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sdkWinmd).Hash
if ($sdkWinmdHash -ne '8A08940ADCDE6CA9A4CAEB9B0F701F90CEA05FE4A170ED70B5520562DF2098F1') {
    throw "Unexpected WMS SDK WINMD hash: $sdkWinmdHash"
}

Write-Host "WMS SDK WINMD: verified $sdkWinmdHash"
Write-Host "Dependencies ready at $Destination"
