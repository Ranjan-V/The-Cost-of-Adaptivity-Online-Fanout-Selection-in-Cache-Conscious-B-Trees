param([switch]$Execute, [string]$Python='python', [string]$SourceId='AUTO')
$ErrorActionPreference = 'Stop'
$tasks = @('run_correctness.ps1','run_static_sweep.ps1')
foreach ($task in $tasks) {
    Write-Host $task
    if ($task -eq 'run_correctness.ps1') {
        & (Join-Path $PSScriptRoot $task) -Execute:$Execute
    } else {
        & (Join-Path $PSScriptRoot $task) -Execute:$Execute -Python $Python -SourceId $SourceId
    }
    if ($LASTEXITCODE -ne 0) { throw "Stopped at $task" }
}
Write-Host 'STOP: review measured static results, run the dynamic-broad and phase-oracle grids, validate pairs, and derive the phase oracle before unblocking selected/perfect variants.'
Write-Host 'Continue with EXECUTION_CHECKLIST.md; this entry point intentionally does not auto-select regimes or invent oracle targets.'
