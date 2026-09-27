param([switch]$Execute, [string]$Python='python', [string]$SourceId='AUTO')
& (Join-Path $PSScriptRoot 'run_campaign.ps1') -Config 'experiments\configs\monitor_ladder.json' -Python $Python -SourceId $SourceId -Execute:$Execute
if ($LASTEXITCODE -ne 0) { throw 'Monitor ablation failed' }
