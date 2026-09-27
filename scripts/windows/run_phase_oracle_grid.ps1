param([switch]$Execute, [string]$Python='python', [string]$SourceId='AUTO')
& (Join-Path $PSScriptRoot 'run_campaign.ps1') -Config 'experiments\configs\phase_length_oracle_grid.json' -Python $Python -SourceId $SourceId -Execute:$Execute
if ($LASTEXITCODE -ne 0) { throw 'Phase oracle grid failed' }
