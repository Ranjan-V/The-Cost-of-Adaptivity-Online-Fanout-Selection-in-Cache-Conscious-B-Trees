param(
    [Parameter(Mandatory=$true)][string]$Config,
    [string]$Python = 'python',
    [string]$SourceId = 'AUTO',
    [switch]$Execute
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$binary = Join-Path $root 'build\prepared\bench_unified.exe'
$runner = Join-Path $root 'experiments\campaign.py'
$configPath = Join-Path $root $Config
$outDir = Join-Path $root 'results\raw\local'
$args = @($runner,'--config',$configPath,'--binary',$binary,'--out-dir',$outDir,'--source-id',$SourceId)
$env:CABTREE_CXXFLAGS = '-std=c++11 -O3 -march=native -DNDEBUG'
if ($Execute) { $args += '--execute' }
Write-Host $Python ($args -join ' ')
& $Python @args
if ($LASTEXITCODE -ne 0) { throw "Campaign failed: $Config" }
