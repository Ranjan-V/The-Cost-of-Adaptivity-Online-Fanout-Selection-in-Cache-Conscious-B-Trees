param([switch]$Execute, [string]$Python='python', [string]$SourceId='AUTO')
& (Join-Path $PSScriptRoot 'run_campaign.ps1') -Config 'experiments\configs\dynamic\selected_template.json' -Python $Python -SourceId $SourceId -Execute:$Execute
if ($LASTEXITCODE -ne 0) { throw 'Dynamic selected campaign failed' }
