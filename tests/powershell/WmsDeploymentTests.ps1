param([Parameter(Mandatory)][string]$TestRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$scriptPath = Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'tools\SetWmsAppLocalDeployment.ps1'
$folder = Join-Path $TestRoot 'portable'
New-Item -ItemType Directory -Force -Path $folder | Out-Null
Set-Content -LiteralPath (Join-Path $folder 'TAUREON-Synth-Tool-V4.exe') -Value 'fixture executable, never run'
$preserved = Join-Path $folder 'user.syx'
[IO.File]::WriteAllBytes($preserved, [byte[]]@(0xF0, 0x7D, 0x01, 0xF7))
$before = (Get-FileHash -LiteralPath $preserved).Hash
$names = @('Windows.Devices.Midi2.dll', 'Windows.Devices.Midi2.pri',
    'Windows.Devices.Midi2.winmd', 'Windows.Devices.Midi2.pdb', 'WMS-PREVIEW-NOTICE.txt')
foreach ($name in $names) { Set-Content -LiteralPath (Join-Path $folder $name) -Value 'old preview' }
& $scriptPath -Destination $folder -Mode SystemOnly
foreach ($name in $names) {
    if (Test-Path -LiteralPath (Join-Path $folder $name)) { throw "Stale file remains: $name" }
}
& $scriptPath -Destination $folder -Mode SystemOnly
if ((Get-FileHash -LiteralPath $preserved).Hash -ne $before) { throw 'User payload changed' }
if (-not (Test-Path -LiteralPath (Join-Path $folder 'TAUREON-Synth-Tool-V4.exe'))) { throw 'Executable removed' }

# Fail before partial cleanup if one of the target names is a directory.
Set-Content -LiteralPath (Join-Path $folder $names[0]) -Value 'must survive failed preflight'
$directoryTarget = Join-Path $folder $names[1]
New-Item -ItemType Directory -Force -Path $directoryTarget | Out-Null
$rejected = $false
try { & $scriptPath -Destination $folder -Mode SystemOnly } catch { $rejected = $true }
if (-not $rejected -or -not (Test-Path -LiteralPath (Join-Path $folder $names[0]))) {
    throw 'Unsafe directory target was not rejected before mutation'
}
Remove-Item -LiteralPath $directoryTarget

# A missing/modified dependency (or unsupported host OS) must not replace the old DLL.
$rejected = $false
try { & $scriptPath -Destination $folder -Mode Preview -DependencyRoot (Join-Path $TestRoot 'missing') }
catch { $rejected = $true }
if (-not $rejected -or (Get-Content -LiteralPath (Join-Path $folder $names[0])) -ne 'must survive failed preflight') {
    throw 'Failed preview input check changed the installed files'
}
& $scriptPath -Destination $folder -Mode SystemOnly
Write-Host 'Portable preview removal, idempotence, payload preservation and failed-preflight checks passed'

# Load the same transaction function used by Preview; synthetic files make output
# failure coverage possible even on a hosted Windows version below the API floor.
. $scriptPath -Destination $folder -Mode SystemOnly
$sourceFolder = Join-Path $TestRoot 'transaction-sources'
New-Item -ItemType Directory -Force -Path $sourceFolder | Out-Null
$transactionNames = @('Windows-MIDI-Services-MIT.txt', 'Windows.Devices.Midi2.dll',
    'WMS-PREVIEW-NOTICE.txt', 'Windows.Devices.Midi2.pri')
$sources = @()
foreach ($name in $transactionNames) {
    $source = Join-Path $sourceFolder $name
    Set-Content -LiteralPath $source -Value "new $name"
    $sources += $source
    Set-Content -LiteralPath (Join-Path $folder $name) -Value "old $name"
}

# A late locked PRI fails after three replacements: all originals must come back.
$locked = [IO.File]::Open((Join-Path $folder 'Windows.Devices.Midi2.pri'),
    [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    $rejected = $false
    try { Invoke-WmsPreviewTransaction -Directory $folder -Sources $sources }
    catch {
        if ($_.Exception.Message -notlike '*previous files restored*') { throw }
        $rejected = $true
    }
    if (-not $rejected) { throw 'Locked output did not fail the transaction' }
} finally { $locked.Dispose() }
foreach ($name in $transactionNames) {
    if ((Get-Content -LiteralPath (Join-Path $folder $name)) -ne "old $name") {
        throw "Failed output transaction did not restore $name"
    }
}

# Roll back a newly created file too; it must not remain after the late failure.
Remove-Item -LiteralPath (Join-Path $folder 'Windows.Devices.Midi2.dll')
$locked = [IO.File]::Open((Join-Path $folder 'Windows.Devices.Midi2.pri'),
    [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    $rejected = $false
    try { Invoke-WmsPreviewTransaction -Directory $folder -Sources $sources }
    catch { $rejected = $_.Exception.Message -like '*previous files restored*' }
    if (-not $rejected) { throw 'New-file rollback did not report restoration' }
} finally { $locked.Dispose() }
if (Test-Path -LiteralPath (Join-Path $folder 'Windows.Devices.Midi2.dll')) {
    throw 'Failed output transaction retained a newly created DLL'
}
foreach ($name in $transactionNames | Where-Object { $_ -ne 'Windows.Devices.Midi2.dll' }) {
    if ((Get-Content -LiteralPath (Join-Path $folder $name)) -ne "old $name") {
        throw "New-file rollback did not restore $name"
    }
}

$readOnlyPath = Join-Path $folder 'Windows.Devices.Midi2.pri'
[IO.File]::SetAttributes($readOnlyPath, [IO.FileAttributes]::ReadOnly)
try {
    $rejected = $false
    try { Invoke-WmsPreviewTransaction -Directory $folder -Sources $sources }
    catch { $rejected = $_.Exception.Message -like '*not a writable regular file*' }
    if (-not $rejected -or (Test-Path -LiteralPath (Join-Path $folder 'Windows.Devices.Midi2.dll'))) {
        throw 'Read-only output was not rejected before replacement'
    }
} finally { [IO.File]::SetAttributes($readOnlyPath, [IO.FileAttributes]::Normal) }

Invoke-WmsPreviewTransaction -Directory $folder -Sources $sources
foreach ($name in $transactionNames) {
    if ((Get-Content -LiteralPath (Join-Path $folder $name)) -ne "new $name") { throw "Commit failed: $name" }
}
if (Get-ChildItem -LiteralPath $folder -Directory -Filter '.wms-deployment-*') {
    throw 'Successful update or rollback retained a temporary transaction directory'
}
if ((Get-FileHash -LiteralPath $preserved).Hash -ne $before) { throw 'Transaction changed user payload' }
& $scriptPath -Destination $folder -Mode SystemOnly
Write-Host 'Late output failure restores original files, removes new files and preserves user data; commit passed'
