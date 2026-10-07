param([string]$Output = '')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $Output) { $Output = Join-Path $root 'kaggle_upload_phase_length_v2.zip' }
if (-not [System.IO.Path]::IsPathRooted($Output)) { $Output = Join-Path (Get-Location) $Output }
$commit = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or -not $commit) { throw 'Cannot resolve Git commit' }
$dirty = if (& git -C $root status --porcelain) { 'yes' } else { 'no' }
$staging = Join-Path ([System.IO.Path]::GetTempPath()) ('cabtree-phase-' + [guid]::NewGuid().ToString('N'))
$tree = Join-Path $staging 'tree'
New-Item -ItemType Directory -Force -Path $tree | Out-Null

function Copy-ProjectFile([string]$RelativePath) {
    $source = Join-Path $root $RelativePath
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing package file: $RelativePath" }
    $destination = Join-Path $tree $RelativePath
    New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination
}

try {
    $files = @(
        'README.md', 'benchmarks\benchmark_unified.cpp', 'tests\test_v2.cpp',
        'tests\test_utils.h', 'experiments\campaign.py',
        'experiments\configs\phase_length_v2_oracle_grid.json',
        'experiments\configs\phase_length_v2.json',
        'scripts\linux\build_release.sh', 'scripts\linux\run_campaign.sh',
        'scripts\linux\run_phase_length_v2_oracle.sh',
        'scripts\linux\run_phase_length_v2.sh', 'analysis\phase_oracle.py',
        'analysis\phase_length_break_even.py',
        'docs\PHASE_LENGTH_V2_CAMPAIGN.md',
        'kaggle\kaggle_phase_length_v2.ipynb'
    )
    foreach ($file in $files) { Copy-ProjectFile $file }
    Copy-Item -LiteralPath (Join-Path $root 'include') -Destination (Join-Path $tree 'include') -Recurse
    Set-Content -LiteralPath (Join-Path $tree 'campaign_git_commit.txt') -Value $commit -Encoding ascii
    @"
Phase-Length V2 Execution Package
Base Git commit: $commit
Packaged working tree differs from base commit: $dirty
Stage A: 150 STATIC-REGIONAL oracle-grid runs
Stage B: 375 STATIC-REGIONAL/PERFECT-V2/ADAPT-V2 runs
Total expected runs: 525
Accelerator: None (CPU only)
Notebook: kaggle/kaggle_phase_length_v2.ipynb
"@ | Set-Content -LiteralPath (Join-Path $tree 'PHASE_LENGTH_V2_PACKAGE_MANIFEST.txt') -Encoding ascii
    if (Test-Path -LiteralPath $Output) { Remove-Item -LiteralPath $Output }
    Compress-Archive -Path (Join-Path $tree '*') -DestinationPath $Output
    Write-Host "Created $Output from commit $commit"
}
finally {
    if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
}
