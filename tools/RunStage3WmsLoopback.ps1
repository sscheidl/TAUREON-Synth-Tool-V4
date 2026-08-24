[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Executable,

    [ValidateRange(1, 1000)]
    [int]$Cycles = 100
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$midiConsole = 'C:\Program Files\Windows MIDI Services\Tools\Console\midi.exe'
if (-not (Test-Path -LiteralPath $midiConsole -PathType Leaf)) {
    Write-Host 'SKIP: installed Windows MIDI Services console is unavailable.'
    exit 77
}
if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "Stage 3 integration executable not found: $Executable"
}

$suffix = [guid]::NewGuid().ToString('N').Substring(0, 6).ToUpperInvariant()
$nameA = "TAUREON S3 WMS A $suffix"
$nameB = "TAUREON S3 WMS B $suffix"
$association = $null

function Remove-Loopback {
    if ($null -eq $script:association) { return }
    & $midiConsole loopback remove --association-id $script:association
    if ($LASTEXITCODE -ne 0) { throw "Failed to remove WMS loopback $script:association" }
    $script:association = $null
}

& $Executable --assert-absent $nameA $nameB
if ($LASTEXITCODE -eq 77) { exit 77 }
if ($LASTEXITCODE -ne 0) { throw 'Unique Stage 3 loopback names already exist.' }

try {
    $script:association = [guid]::NewGuid().ToString('B')
    $identifier = "taureon-stage3-$([guid]::NewGuid().ToString('N'))"
    & $midiConsole loopback create --name-a $nameA --name-b $nameB `
        --association-id $script:association --unique-identifier $identifier
    if ($LASTEXITCODE -ne 0) { throw 'Failed to create temporary Stage 3 WMS loopback.' }

    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        & $Executable --lifecycle $nameB $nameA $Cycles
        if ($LASTEXITCODE -eq 0) { break }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    if ($LASTEXITCODE -ne 0) { throw 'Stage 3 realtime transport regression failed.' }
}
finally {
    Remove-Loopback
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        & $Executable --assert-absent $nameA $nameB
        if ($LASTEXITCODE -eq 0) { break }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    if ($LASTEXITCODE -ne 0) { throw 'Temporary Stage 3 loopback remains visible after cleanup.' }
}
