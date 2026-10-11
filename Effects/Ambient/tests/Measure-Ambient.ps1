param(
    [ValidateSet('baseline','optimized')] [string]$Flavor = 'optimized',
    [ValidateSet('idle','move','resize','gdi-move')] [string]$Scenario = 'move',
    [ValidateRange(1,60)] [int]$Fps = 60,
    [ValidateSet('average','copy')] [string]$EffectMode = 'average',
    [ValidateRange(5,300)] [int]$Seconds = 12,
    [string]$OutputRoot,
    [string]$Runtime = 'C:\Program Files\OpenRGB'
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
if (-not $OutputRoot) { $OutputRoot = Join-Path (Split-Path $repoRoot -Parent) 'artifacts/benchmark' }
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$benchmarkExe = Join-Path $repoRoot "build/benchmark-$Flavor/AmbientBenchmark-$Flavor.exe"
if (-not (Test-Path -LiteralPath $benchmarkExe)) { throw "Build the benchmark first: $benchmarkExe" }
$env:PATH = "$Runtime;$env:PATH"
$env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $Runtime 'platforms'
$env:QT_FORCE_STDERR_LOGGING = '1'
$env:OPENRGB_AMBIENT_METRICS = '1'
$prefix = Join-Path $OutputRoot "$Flavor-$Scenario-${Fps}fps-$EffectMode"
$dwmIds = @(Get-Process dwm -ErrorAction SilentlyContinue | ForEach-Object { $_.Id })
$process = Start-Process -FilePath $benchmarkExe -ArgumentList @($Scenario,$Fps,$Seconds,$EffectMode) -PassThru -WindowStyle Hidden -RedirectStandardOutput "$prefix.json" -RedirectStandardError "$prefix-metrics.log"
$gpuSamples = [System.Collections.Generic.List[object]]::new()
$counterErrors = [System.Collections.Generic.List[string]]::new()
try {
    while (-not $process.HasExited) {
        try {
            $counters = (Get-Counter -Counter '\GPU Engine(*)\Utilization Percentage' -SampleInterval 1 -MaxSamples 1 -ErrorAction Stop).CounterSamples
            $owned = @($counters | Where-Object { $_.InstanceName.StartsWith("pid_$($process.Id)_") })
            $dwm = @($counters | Where-Object {
                foreach ($dwmId in $dwmIds) { if ($_.InstanceName.StartsWith("pid_${dwmId}_")) { return $true } }
                return $false
            })
            $gpuSamples.Add([pscustomobject]@{
                timestamp = (Get-Date).ToString('o')
                process_engine_count = $owned.Count
                process_busiest_engine_percent = if ($owned.Count) { ($owned | Measure-Object -Property CookedValue -Maximum).Maximum } else { $null }
                dwm_busiest_engine_percent = if ($dwm.Count) { ($dwm | Measure-Object -Property CookedValue -Maximum).Maximum } else { $null }
            })
        } catch { $counterErrors.Add($_.Exception.Message); Start-Sleep -Milliseconds 500 }
    }
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw "Benchmark failed with exit code $($process.ExitCode): $prefix-metrics.log" }
} finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}
[pscustomobject]@{
    benchmark_pid = $process.Id
    counter = '\GPU Engine(*)\Utilization Percentage'
    definition = 'Maximum per-process engine utilization, not a sum of overlapping GPU engines; missing instances are null.'
    errors = @($counterErrors)
    samples = @($gpuSamples.ToArray())
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$prefix-gpu.json" -Encoding utf8
$result = Get-Content -LiteralPath "$prefix.json" -Raw | ConvertFrom-Json
if ($result.delivered_frames -eq 0) { throw "Invalid benchmark: no desktop frames delivered. $prefix-metrics.log" }
Write-Output ([pscustomobject]@{
    flavor=$Flavor; scenario=$Scenario; fps=$Fps; delivered_fps=$result.delivered_fps
    capture_mean_ms=$result.capture_or_poll_wall_time.mean_ms
    capture_p95_ms=$result.capture_or_poll_wall_time.p95_ms
    cpu_percent_machine=$result.cpu_percent_total_machine
    results="$prefix.json"
})
