param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$rawRoot = Join-Path $root 'results\raw\local'
$processed = Join-Path $root 'results\processed\adaptive_scale_replication'
$analysis = Join-Path $root 'analysis\adaptive_scale_replication.py'
$plot = Join-Path $root 'analysis\plot_adaptive_scale.py'
$figure = Join-Path $processed 'adaptive_scale_replication.pdf'
$archive = Join-Path $root 'adaptive_scale_replication_results.zip'

& $Python $analysis --raw-root $rawRoot --out-dir $processed
if ($LASTEXITCODE -ne 0) { throw 'Replication analysis failed' }
& $Python $plot `
    --summary (Join-Path $processed 'adaptive_scale_replication_summary.csv') `
    --output $figure
if ($LASTEXITCODE -ne 0) { throw 'Replication figure generation failed' }

if (Test-Path -LiteralPath $archive) {
    throw "Archive already exists; preserve it or rename it before finalizing: $archive"
}
$campaignDirs = Get-ChildItem -Path $rawRoot -Directory | ForEach-Object {
    Join-Path $_.FullName 'adaptive_scale_replication'
} | Where-Object { Test-Path -LiteralPath $_ }
if (@($campaignDirs).Count -ne 1) {
    throw "Expected exactly one local replication campaign directory, found $(@($campaignDirs).Count)"
}
$campaignDir = @($campaignDirs)[0]
$archiveInputs = @($campaignDir, $processed)
Compress-Archive -LiteralPath $archiveInputs -DestinationPath $archive
Write-Host "Verified outputs: $processed"
Write-Host "Evidence archive:  $archive"
