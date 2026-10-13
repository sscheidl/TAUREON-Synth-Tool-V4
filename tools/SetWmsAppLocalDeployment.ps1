[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Destination,
    [Parameter(Mandatory)][ValidateSet('Preview', 'SystemOnly')][string]$Mode,
    [string]$DependencyRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build\dependencies\wms-preview11')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# An explicit existing TAUREON folder only; never follow links into system files.
$folder = Get-Item -LiteralPath $Destination
if (-not $folder.PSIsContainer) { throw 'Destination must be an existing TAUREON folder' }
for ($ancestor = $folder; $null -ne $ancestor; $ancestor = $ancestor.Parent) {
    if ($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw 'Deployment through a linked directory is not supported'
    }
}
$destinationPath = $folder.FullName
foreach ($systemRoot in @($env:windir, $env:ProgramFiles, ${env:ProgramFiles(x86)})) {
    if ($systemRoot -and ($destinationPath -eq $systemRoot -or
        $destinationPath.StartsWith($systemRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase))) {
        throw 'Use a portable TAUREON folder outside Windows and Program Files'
    }
}
if (-not ((Test-Path -LiteralPath (Join-Path $destinationPath 'TAUREON-Synth-Tool-V4.exe') -PathType Leaf) -or
          (Test-Path -LiteralPath (Join-Path $destinationPath 'taureon_app.exe') -PathType Leaf))) {
    throw 'Destination does not contain a TAUREON application executable'
}

$previewFiles = @('Windows.Devices.Midi2.dll', 'Windows.Devices.Midi2.pri',
    'Windows.Devices.Midi2.winmd', 'Windows.Devices.Midi2.pdb', 'WMS-PREVIEW-NOTICE.txt')
$writeFiles = $previewFiles + @('Windows-MIDI-Services-MIT.txt')
foreach ($name in $writeFiles) {
    $path = Join-Path $destinationPath $name
    if (Test-Path -LiteralPath $path) {
        $item = Get-Item -LiteralPath $path
        if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Deployment target must be a regular file: $name"
        }
    }
}

if ($Mode -eq 'SystemOnly') {
    # This is also the migration path for users who extracted over an older ZIP.
    foreach ($name in $previewFiles) {
        $path = Join-Path $destinationPath $name
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    Write-Host 'Removed app-local WMS preview files; Windows API or WinMM will be used'
    return
}

if ([Environment]::OSVersion.Version.Build -lt 26200) {
    throw 'Preview deployment requires Windows 11 25H2 or newer; use SystemOnly and WinMM'
}
$runtimeRoot = Join-Path $DependencyRoot 'package\runtimes\win-x64\native'
$runtimeHashes = @{
    'Windows.Devices.Midi2.dll' = 'DB3544CAC751F174DA338FB43E754974A067432CE30ECE9845CFCF3B3DAA4CDC'
    'Windows.Devices.Midi2.pri' = 'D04A030D84C77C6BE8C0F2018A6D70FCD142AB5C940644B74223523F975DA9FA'
}
# Validate both inputs before writing either file. The metadata/PDB are not runtime inputs.
foreach ($name in $runtimeHashes.Keys) {
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $runtimeRoot $name)).Hash
    if ($hash -ne $runtimeHashes[$name]) { throw "WMS Preview 11 runtime hash mismatch: $name" }
}
$projectRoot = Split-Path -Parent $PSScriptRoot
$licensePath = Join-Path $projectRoot 'docs\licenses\Windows-MIDI-Services-MIT.txt'
$noticePath = Join-Path $projectRoot 'docs\release\WMS-PREVIEW-NOTICE.txt'
foreach ($path in @($licensePath, $noticePath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing deployment notice: $path" }
}
foreach ($name in $runtimeHashes.Keys) {
    Copy-Item -LiteralPath (Join-Path $runtimeRoot $name) -Destination (Join-Path $destinationPath $name)
}
Copy-Item -LiteralPath $licensePath -Destination $destinationPath
Copy-Item -LiteralPath $noticePath -Destination $destinationPath
Write-Host 'Verified Preview 11 DLL/PRI deployed; Go-live obligations still apply before distribution'
