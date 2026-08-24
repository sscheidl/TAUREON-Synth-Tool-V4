[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$projectRoot = Split-Path -Parent $PSScriptRoot
$cachePath = Join-Path $projectRoot 'build\vs2022-x64\CMakeCache.txt'
$applicationPath = Join-Path $projectRoot ("build\vs2022-x64\src\{0}\taureon_app.exe" -f $Configuration)

if (-not (Test-Path -LiteralPath $cachePath)) {
    throw "No configured build was found at $cachePath. Run 'cmake --preset vs2022-x64' first."
}
if (-not (Test-Path -LiteralPath $applicationPath)) {
    throw "No $Configuration application build was found at $applicationPath. Build it first."
}

$qt6DirLine = Select-String -LiteralPath $cachePath -Pattern '^Qt6_DIR:PATH=(.+)$' | Select-Object -First 1
if (-not $qt6DirLine) {
    throw "Qt6_DIR is not present in the configured CMake cache. Reconfigure the project."
}
$qt6Dir = $qt6DirLine.Matches[0].Groups[1].Value
$qtBin = [System.IO.Path]::GetFullPath((Join-Path $qt6Dir '..\..\..\bin'))
if (-not (Test-Path -LiteralPath (Join-Path $qtBin 'Qt6Core.dll'))) {
    throw "The configured Qt runtime bin directory is invalid: $qtBin"
}

$env:PATH = "$qtBin;$env:PATH"
& $applicationPath
exit $LASTEXITCODE
