$ErrorActionPreference='Stop'
$installed=Split-Path -Parent $PSScriptRoot
$directory=Join-Path $env:LOCALAPPDATA 'OpenRGB-LightSync'
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$log=Join-Path $directory 'ResumeRecovery.log'
$mutex=New-Object Threading.Mutex($false,'Local\OpenRGBLightSync.ResumeRecovery')
$owned=$false
try{
 try{$owned=$mutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$owned=$true}
 if(-not $owned){return}
 $session=[Diagnostics.Process]::GetCurrentProcess().SessionId
 if(Get-Process OpenRGB -ErrorAction SilentlyContinue | Where-Object SessionId -eq $session){Add-Content $log ((Get-Date -Format o)+' OpenRGB already running; skipped.');return}
 $exe=Join-Path $installed 'OpenRGB.exe'
 if(-not(Test-Path -LiteralPath $exe)){throw 'OpenRGB.exe missing.'}
 if((Get-AuthenticodeSignature $exe).Status -ne 'Valid'){throw 'OpenRGB signature is invalid; recovery skipped.'}
 Start-Process -FilePath $exe -WorkingDirectory $installed -ArgumentList '--startminimized' -WindowStyle Hidden
 Add-Content $log ((Get-Date -Format o)+' OpenRGB restarted after resume.')
}catch{Add-Content $log ((Get-Date -Format o)+' Recovery error: '+$_.Exception.Message);throw}
finally{if($owned){$mutex.ReleaseMutex()};$mutex.Dispose()}

