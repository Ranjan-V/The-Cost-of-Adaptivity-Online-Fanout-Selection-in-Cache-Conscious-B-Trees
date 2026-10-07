param(
    [switch]$Execute,
    [string]$Python = 'python',
    [string]$SourceId = 'AUTO'
)
$ErrorActionPreference = 'Stop'
$runner = Join-Path $PSScriptRoot 'run_campaign.ps1'
& $runner `
    -Config 'experiments\configs\adaptive_scale_replication.json' `
    -Python $Python `
    -SourceId $SourceId `
    -Execute:$Execute
if ($LASTEXITCODE -ne 0) { throw 'Adaptive-scale replication failed' }
