param([Parameter(Mandatory=$true)][string]$Probe,
      [Parameter(Mandatory=$true)][string]$Trace,
      [ValidateRange(1,10)][int]$Fps=6,
      [ValidateRange(1,1800)][int]$Seconds=300,
      [switch]$ModeOnce)
$ErrorActionPreference='Stop'
$tracePath=[IO.Path]::GetFullPath($Trace)
$probePath=[IO.Path]::GetFullPath($Probe)
if($tracePath.Contains('"') -or $probePath.Contains('"')) {throw 'Invalid path'}
if(Test-Path -LiteralPath $tracePath) {throw 'Trace already exists'}
$metricsPath=$tracePath+'.metrics.jsonl'
if(Test-Path -LiteralPath $metricsPath) {throw 'Metrics already exist'}
$arguments=@("$Fps","$Seconds",('"'+$tracePath+'"'))
if($ModeOnce) {$arguments+='mode-once'}
$started=[DateTime]::UtcNow
$run=Start-Process -FilePath $probePath -ArgumentList $arguments -WindowStyle Hidden -PassThru
$writer=New-Object IO.StreamWriter($metricsPath,$false,(New-Object Text.UTF8Encoding($false)))
try {
 while(-not $run.HasExited) {
  $run.Refresh()
  $sample=[ordered]@{event='sample';utc=[DateTime]::UtcNow.ToString('o');pid=$run.Id;
    cpu_seconds=$run.TotalProcessorTime.TotalSeconds;working_set_bytes=$run.WorkingSet64;
    responding=$run.Responding;logical_processors=[Environment]::ProcessorCount}
  # OS performance counters only; no vendor sensor/protocol probing.
  try {
   $engines=Get-CimInstance -ClassName Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -ErrorAction Stop
   $usage=@($engines | Where-Object {$_.UtilizationPercentage -gt 0} | Select-Object Name,UtilizationPercentage)
   $sample['gpu_engines_nonzero']=$usage
  } catch {$sample['gpu_counter_error']=$_.Exception.Message}
  $writer.WriteLine(($sample|ConvertTo-Json -Depth 5 -Compress));$writer.Flush()
  [void]$run.WaitForExit(5000)
 }
 $run.WaitForExit();$run.Refresh()
 $elapsed=([DateTime]::UtcNow-$started).TotalSeconds
 $cpu=$run.TotalProcessorTime.TotalSeconds
 $writer.WriteLine(([ordered]@{event='end';utc=[DateTime]::UtcNow.ToString('o');exit_code=$run.ExitCode;
   wall_seconds=$elapsed;cpu_seconds=$cpu;process_cpu_percent_machine=100*$cpu/$elapsed/[Environment]::ProcessorCount;
   peak_working_set_bytes=$run.PeakWorkingSet64}|ConvertTo-Json -Compress))
 Write-Output "Probe exited $($run.ExitCode); wall seconds $elapsed; CPU seconds $cpu"
 exit $run.ExitCode
} finally {$writer.Dispose()}
