[CmdletBinding()]
param(
    [string]$Destination
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$projectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Join-Path $projectRoot 'build\dependencies\wms-preview11'
}

$packages = @(
    @{
        Name = 'Windows.Devices.Midi2'
        Version = '0.99.92-preview.11'
        Uri = 'https://github.com/microsoft/MIDI/releases/download/inbox-preview-11/Windows.Devices.Midi2.0.99.92-preview.11.nupkg'
        FileName = 'Windows.Devices.Midi2.0.99.92-preview.11.nupkg'
        Sha256 = '13AFC6E6DA2ADAC309AB8C174352F46F1C0EBE2D79E65475BD6274DEFB73DF74'
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
if ($sdkWinmdHash -ne '49AB3BBE0954BF4F4D0D1AB933564974495762A11B9B02602F493C6C755C47BC') {
    throw "Unexpected WMS SDK WINMD hash: $sdkWinmdHash"
}

Write-Host "WMS SDK WINMD: verified $sdkWinmdHash"
$runtimeHashes = @{
    'Windows.Devices.Midi2.dll' = 'DB3544CAC751F174DA338FB43E754974A067432CE30ECE9845CFCF3B3DAA4CDC'
    'Windows.Devices.Midi2.pri' = 'D04A030D84C77C6BE8C0F2018A6D70FCD142AB5C940644B74223523F975DA9FA'
}
foreach ($name in $runtimeHashes.Keys) {
    $runtimePath = Join-Path $Destination "package\runtimes\win-x64\native\$name"
    $runtimeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $runtimePath).Hash
    if ($runtimeHash -ne $runtimeHashes[$name]) { throw "Unexpected WMS runtime hash: $name" }
    Write-Host "WMS x64 ${name}: verified $runtimeHash"
}
Write-Host "Dependencies ready at $Destination"
