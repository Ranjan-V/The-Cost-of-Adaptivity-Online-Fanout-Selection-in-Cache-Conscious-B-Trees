param([switch]$Execute, [string]$Python='python', [string]$SourceId='AUTO')
& (Join-Path $PSScriptRoot 'run_campaign.ps1') -Config 'experiments\configs\static_opportunity\grid.json' -Python $Python -SourceId $SourceId -Execute:$Execute
if ($LASTEXITCODE -ne 0) { throw 'Static sweep failed' }
