param([switch]$Execute)

$ErrorActionPreference = 'Stop'
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$exe = Join-Path $root 'build\prepared\bench_unified.exe'
$runStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$outputDirectory = Join-Path $root (Join-Path 'results\raw\local\correctness-smoke' $runStamp)
$summaryPath = Join-Path $root 'results\processed\v2_correctness_smoke.csv'
$variants = @('STATIC', 'ADAPT-V1', 'ADAPT-V2')

if (-not (Test-Path -LiteralPath $exe)) {
    throw "Missing $exe. Run scripts\windows\build_release.ps1 -Execute first."
}

Write-Host "Executable: $exe"
Write-Host "Output:     $outputDirectory"
if (-not $Execute) {
    Write-Host 'Preview only. Re-run with -Execute to perform the smoke checks.'
    exit 0
}

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $summaryPath) | Out-Null

foreach ($variant in $variants) {
    $safeName = $variant.ToLowerInvariant().Replace('-', '_')
    $output = Join-Path $outputDirectory ($safeName + '.csv')
    $arguments = @(
        '--variant', $variant,
        '--output', $output,
        '--run-id', ('correctness-' + $runStamp + '-' + $safeName),
        '--family', 'shifting',
        '--experiment-family', 'correctness-smoke',
        '--machine', 'laptop-i5-12500h',
        '--environment', 'local-windows',
        '--source-id', 'correctness-v2',
        '--compiler-flags', '-O3-march-native-DNDEBUG',
        '--cpu-model', 'i5-12500H',
        '--records', '10000',
        '--operations', '100000',
        '--segments', '8',
        '--phase-length', '20000',
        '--adapt-interval', '5000',
        '--sample-rate', '32',
        '--warmup', '1000',
        '--latency-sampling-rate', '128',
        '--threads', '1',
        '--repetition', '0',
        '--fanout', '64',
        '--seed', '20260921',
        '--zipf', '0.99',
        '--reads', '0.95',
        '--updates', '0.05',
        '--hot-fraction', '0.20',
        '--hot-segment', '0',
        '--candidate-fanouts', '8:16:32:64:128:256'
    )

    Write-Host "`n=== Smoke test: $variant ===" -ForegroundColor Cyan
    & $exe @arguments
    $runExitCode = $LASTEXITCODE
    if ($runExitCode -ne 0) {
        throw "$variant smoke test failed with exit code $runExitCode"
    }
}

$resultPaths = foreach ($variant in $variants) {
    $safeName = $variant.ToLowerInvariant().Replace('-', '_')
    Join-Path $outputDirectory ($safeName + '.csv')
}
$rows = $resultPaths | ForEach-Object { Import-Csv -LiteralPath $_ }

if ($rows.Count -ne $variants.Count) {
    throw "Expected $($variants.Count) result rows, found $($rows.Count)"
}
if (($rows.checksum | Sort-Object -Unique).Count -ne 1) {
    throw 'STATIC/V1/V2 checksum mismatch'
}
if (($rows.workload_fingerprint | Sort-Object -Unique).Count -ne 1) {
    throw 'STATIC/V1/V2 workload fingerprint mismatch'
}
if (($rows | Where-Object { [int64]$_.misses -ne 0 }).Count -ne 0) {
    throw 'One or more variants reported missing keys'
}
if (($rows | Where-Object { $_.invariant_status -ne 'NOT_CHECKED' }).Count -ne 0) {
    throw 'Unexpected invariant status in timing output'
}

$report = $rows | Select-Object variant, checksum, misses, workload_fingerprint,
    invariant_status, rebuild_count, throughput_ops_sec
$report | Format-Table -AutoSize
$report | Export-Csv -LiteralPath $summaryPath -NoTypeInformation
Write-Host "`nSTATIC, ADAPT-V1, and ADAPT-V2 agree." -ForegroundColor Green
Write-Host 'Structural invariants passed separately in test_v2.exe; timing rows correctly report NOT_CHECKED.'
Write-Host "Summary: $summaryPath"
