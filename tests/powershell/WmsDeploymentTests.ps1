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
