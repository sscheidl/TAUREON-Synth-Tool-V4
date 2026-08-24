[CmdletBinding()]
param(
    [ValidateRange(1, 1000)]
    [int]$Cycles = 100,

    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$projectRoot = Split-Path -Parent $PSScriptRoot
$midiConsole = 'C:\Program Files\Windows MIDI Services\Tools\Console\midi.exe'
$spike = Join-Path $projectRoot "build\vs2022-x64\spikes\winmm_direct\$Configuration\taureon_winmm_spike.exe"
$correlation = Join-Path $projectRoot "build\vs2022-x64\spikes\wms_direct\$Configuration\taureon_wms_winmm_correlation.exe"

foreach ($requiredFile in @($midiConsole, $spike, $correlation)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required Stage 1 executable not found: $requiredFile"
    }
}

$suffix = [guid]::NewGuid().ToString('N').Substring(0, 6).ToUpperInvariant()
$nameA = "TAUREON S1 WMS A $suffix"
$nameB = "TAUREON S1 WMS B $suffix"
$activeAssociation = $null

function Remove-ActiveLoopback {
    if ($null -ne $script:activeAssociation) {
        & $midiConsole loopback remove --association-id $script:activeAssociation
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to remove WMS loopback association $script:activeAssociation"
        }
        $script:activeAssociation = $null
    }
}

function New-TemporaryLoopback {
    $script:activeAssociation = [guid]::NewGuid().ToString('B')
    $uniqueIdentifier = "taureon-stage1-$([guid]::NewGuid().ToString('N'))"
    & $midiConsole loopback create --name-a $nameA --name-b $nameB `
        --association-id $script:activeAssociation --unique-identifier $uniqueIdentifier
    if ($LASTEXITCODE -ne 0) {
        $script:activeAssociation = $null
        throw 'Failed to create the temporary WMS loopback pair.'
    }
}

function Get-ExactPairInspection {
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        $lines = @(& $spike --inspect-pair $nameB $nameA)
        if ($LASTEXITCODE -eq 0) {
            $jsonLine = $lines | Where-Object { $_ -like '{*' } | Select-Object -Last 1
            if ($jsonLine) {
                return $jsonLine | ConvertFrom-Json
            }
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'Temporary WMS loopback pair did not become exactly and uniquely visible through WinMM.'
}

function Assert-PairAbsent {
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        & $spike --assert-absent $nameA $nameB
        if ($LASTEXITCODE -eq 0) { return }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'Temporary WMS loopback names remain visible through WinMM after removal.'
}

Write-Host "Stage 1 temporary WMS loopback: output '$nameA' -> input '$nameB'"
& $spike --assert-absent $nameA $nameB
if ($LASTEXITCODE -ne 0) {
    throw 'The unique temporary endpoint names were already present before creation.'
}

try {
    New-TemporaryLoopback
    $firstInspection = Get-ExactPairInspection
    $firstInspection | ConvertTo-Json -Compress

    & $correlation $firstInspection.input_index_hint $nameB `
        $firstInspection.output_index_hint $nameA
    if ($LASTEXITCODE -ne 0) {
        Write-Host ('{"event":"winmm_wms_correlation_hypothesis",' +
            '"status":"pinned_rc4_fail_fast",' +
            '"exit_code":' + $LASTEXITCODE + ',' +
            '"transport_gate":false,"architecture_conclusion":false}')
    }

    & $spike --wms-loopback $nameB $nameA $Cycles
    if ($LASTEXITCODE -ne 0) {
        throw "WinMM WMS-loopback spike failed with exit code $LASTEXITCODE."
    }

    Remove-ActiveLoopback
    Assert-PairAbsent

    # Recreate the same persisted names with a new association to exercise disappearance/re-enumeration.
    New-TemporaryLoopback
    $secondInspection = Get-ExactPairInspection
    $secondInspection | ConvertTo-Json -Compress

    $identityFields = @(
        'input_name', 'input_wMid', 'input_wPid', 'input_driver',
        'output_name', 'output_wMid', 'output_wPid', 'output_driver'
    )
    foreach ($field in $identityFields) {
        if ($firstInspection.$field -ne $secondInspection.$field) {
            throw "Persisted identity changed after re-enumeration: $field"
        }
    }
    Write-Host '{"event":"winmm_reenumeration","disappearance_visible":true,"same_composite_identity_resolved":true,"silent_fallback":false}'
}
finally {
    Remove-ActiveLoopback
    Assert-PairAbsent
}

Write-Host '{"event":"temporary_wms_loopback_cleanup","removed":true,"winmm_absent":true}'
