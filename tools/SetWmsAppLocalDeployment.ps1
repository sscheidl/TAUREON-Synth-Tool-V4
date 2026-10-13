[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Destination,
    [Parameter(Mandatory)][ValidateSet('Preview', 'SystemOnly')][string]$Mode,
    [string]$DependencyRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build\dependencies\wms-preview11')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-WmsPreviewTransaction {
    param([string]$Directory, [string[]]$Sources)

    $allowed = @('Windows.Devices.Midi2.dll', 'Windows.Devices.Midi2.pri',
        'Windows-MIDI-Services-MIT.txt', 'WMS-PREVIEW-NOTICE.txt')
    $names = @($Sources | ForEach-Object { [IO.Path]::GetFileName($_) })
    if ($names.Count -ne 4 -or @($names | Select-Object -Unique).Count -ne 4 -or
        @($names | Where-Object { $_ -notin $allowed }).Count -ne 0) {
        throw 'A preview transaction requires exactly the DLL, PRI, license and notice'
    }
    $directoryPath = (Get-Item -LiteralPath $Directory).FullName
    $temporaryPath = [IO.Path]::GetFullPath((Join-Path $directoryPath ('.wms-deployment-' + [Guid]::NewGuid().ToString('N'))))
    if (-not $temporaryPath.StartsWith($directoryPath.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Transaction directory must remain inside the selected TAUREON folder'
    }
    New-Item -ItemType Directory -Path $temporaryPath | Out-Null
    $changed = [Collections.Generic.List[object]]::new()
    $retainRecovery = $false
    try {
        # Stage the complete set on the destination volume before replacing anything.
        foreach ($source in $Sources) {
            $name = [IO.Path]::GetFileName($source)
            $target = Join-Path $directoryPath $name
            if (Test-Path -LiteralPath $target) {
                $item = Get-Item -LiteralPath $target
                if ($item.PSIsContainer -or ($item.Attributes -band
                    ([IO.FileAttributes]::ReparsePoint -bor [IO.FileAttributes]::ReadOnly))) {
                    throw "Preview target is not a writable regular file: $name"
                }
            }
            Copy-Item -LiteralPath $source -Destination (Join-Path $temporaryPath $name)
        }
        foreach ($name in $names) {
            $target = Join-Path $directoryPath $name
            $staged = Join-Path $temporaryPath $name
            $backup = Join-Path $temporaryPath ($name + '.backup')
            $existed = Test-Path -LiteralPath $target
            $change = @{ Target = $target; Backup = $backup; Staged = $staged;
                Existed = $existed; Created = $false }
            $changed.Add($change)
            if ($existed) {
                # Same-volume replacement atomically retains the previous file.
                [IO.File]::Replace($staged, $target, $backup)
            } else {
                [IO.File]::Move($staged, $target)
                $change.Created = $true
            }
        }
    } catch {
        $failure = $_.Exception.Message
        $rollbackErrors = [Collections.Generic.List[string]]::new()
        for ($index = $changed.Count - 1; $index -ge 0; --$index) {
            $change = $changed[$index]
            try {
                if ($change.Existed) {
                    # A failed Replace may already have moved the original to its
                    # backup. Restore it even when that operation did not return.
                    if (Test-Path -LiteralPath $change.Backup -PathType Leaf) {
                        if (Test-Path -LiteralPath $change.Target -PathType Leaf) {
                            [IO.File]::Replace($change.Backup, $change.Target, $change.Staged)
                        } else {
                            [IO.File]::Move($change.Backup, $change.Target)
                        }
                    } elseif (-not (Test-Path -LiteralPath $change.Target -PathType Leaf)) {
                        throw "Original file and backup missing: $($change.Target)"
                    }
                } elseif ($change.Created) {
                    [IO.File]::Delete($change.Target)
                }
            } catch { $rollbackErrors.Add($_.Exception.Message) }
        }
        if ($rollbackErrors.Count -gt 0) {
            $retainRecovery = $true
            throw "Preview update and rollback failed. Keep TAUREON closed; original backups remain at $temporaryPath. Failure: $failure; rollback: $($rollbackErrors -join '; ')"
        }
        throw "Preview update failed; previous files restored. $failure"
    } finally {
        if (-not $retainRecovery) {
            # Only our fixed staging/backup names; never recursively delete user data.
            foreach ($name in $names) {
                foreach ($suffix in @('', '.backup')) {
                    $path = Join-Path $temporaryPath ($name + $suffix)
                    if (Test-Path -LiteralPath $path -PathType Leaf) {
                        try { Remove-Item -LiteralPath $path -Force -ErrorAction Stop }
                        catch { Write-Warning "Deployment temporary file could not be removed: $path" }
                    }
                }
            }
            if ([IO.Directory]::GetFileSystemEntries($temporaryPath).Length -eq 0) {
                [IO.Directory]::Delete($temporaryPath)
            }
        }
    }
}

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
Invoke-WmsPreviewTransaction -Directory $destinationPath -Sources @(
    $licensePath, $noticePath,
    (Join-Path $runtimeRoot 'Windows.Devices.Midi2.pri'),
    (Join-Path $runtimeRoot 'Windows.Devices.Midi2.dll'))
Write-Host 'Verified Preview 11 DLL/PRI deployed; Go-live obligations still apply before distribution'
