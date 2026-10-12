param([string]$BackupDirectory="$env:LOCALAPPDATA\OpenRGB-LightSync\PolicyBackup",[string]$Restore)
$ErrorActionPreference='Stop'
if($Restore){& reg.exe import $Restore;if($LASTEXITCODE -ne 0){throw 'Restoration failed'};return}
$package=Get-AppxPackage -Name OpenRGB.LightSync
if(-not $package){throw 'Install OpenRGB.LightSync first, under this Windows user.'}
$family=$package.PackageFamilyName
$devices=Get-ChildItem HKCU:\Software\Microsoft\Lighting\Devices | Where-Object PSChildName -Match 'VID_048D&PID_C193'
New-Item -ItemType Directory -Path $BackupDirectory -Force | Out-Null
foreach($device in $devices){
 $key=$device.OpenSubKey('Providers',$true)
 if(-not $key){continue}
 try {
  $names=@($key.GetValueNames() | Where-Object {$_ -match '^\d+$'} | Sort-Object {[int]$_})
  $values=@($names | ForEach-Object {[string]$key.GetValue($_)})
  if($values -notcontains $family){throw 'OpenRGB is absent from registered providers. Start it once, then retry.'}
  $backup=Join-Path $BackupDirectory ('Lenovo-priority-'+(Get-Date -Format yyyyMMdd-HHmmss)+'.reg')
  & reg.exe export ($device.Name+'\Providers') $backup /y | Out-Null
  if($LASTEXITCODE -ne 0){throw 'Policy backup failed'}
  $ordered=@($family)+@($values | Where-Object {$_ -ne $family})
  foreach($name in $names){$key.DeleteValue($name)}
  for($i=0;$i -lt $ordered.Count;$i++){$key.SetValue([string]($i+1),$ordered[$i],[Microsoft.Win32.RegistryValueKind]::String)}
  Write-Host "Lenovo background priority: OpenRGB first. Undo: -Restore '$backup'"
 }finally{$key.Dispose()}
}
