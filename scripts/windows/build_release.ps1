param([string]$Compiler = 'g++', [switch]$Execute)
$ErrorActionPreference = 'Stop'
# PowerShell 7 can promote native stderr (including ordinary compiler warnings)
# to ErrorRecord objects when ErrorActionPreference is Stop.  Let the compiler
# exit code, not the output stream, decide whether a build failed.
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$build = Join-Path $root 'build\prepared'
$targets = @(
    @{ Source = 'benchmarks\benchmark_unified.cpp'; Output = 'bench_unified.exe'; Flags = @('-O3','-march=native','-DNDEBUG') },
    @{ Source = 'tests\test_v2.cpp'; Output = 'test_v2.exe'; Flags = @('-O0','-g') }
)
foreach ($item in $targets) {
    $src = Join-Path $root $item.Source
    $out = Join-Path $build $item.Output
    $args = @('-std=c++11','-Wall','-Wextra') + $item.Flags + @($src,'-o',$out,'-lpsapi')
    Write-Host $Compiler ($args -join ' ')
    if ($Execute) {
        New-Item -ItemType Directory -Force -Path $build | Out-Null
        $log = Join-Path $build ($item.Output + '.build.log')
        & $Compiler @args *> $log
        $compilerExitCode = $LASTEXITCODE
        if ($compilerExitCode -ne 0) { Get-Content $log; throw "Build failed ($compilerExitCode): $src" }
        Get-Content $log
    }
}
