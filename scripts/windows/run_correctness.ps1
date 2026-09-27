param([switch]$Execute)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$exe = Join-Path $root 'build\prepared\test_v2.exe'
$log = Join-Path $root 'results\logs\test_v2.log'
Write-Host $exe
if ($Execute) {
    New-Item -ItemType Directory -Force -Path (Split-Path $log) | Out-Null
    & $exe *> $log
    if ($LASTEXITCODE -ne 0) { Get-Content $log; throw 'Correctness test failed' }
    Get-Content $log
}
