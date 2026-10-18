param([string]$AppRoot=(Join-Path $PSScriptRoot '..\app'),[switch]$Remove,[switch]$PrioritizeKeyboard)
$ErrorActionPreference='Stop'
$name='OpenRGB.LightSync.Preview'
if($Remove){
 $package=Get-AppxPackage -Name $name
 if($package){
  $family=$package.PackageFamilyName
  $paths=@('Software\Microsoft\Lighting\Providers')
  foreach($device in Get-ChildItem 'HKCU:\Software\Microsoft\Lighting\Devices' -ErrorAction SilentlyContinue){$paths+=($device.Name.Substring('HKEY_CURRENT_USER\'.Length)+'\Providers')}
  foreach($path in $paths){
   $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($path,$true);if(-not $key){continue}
   try {
    $names=@($key.GetValueNames()|Where-Object {$_ -match '^\d+$'}|Sort-Object {[int]$_})
    $values=@($names|ForEach-Object {[string]$key.GetValue($_)}|Where-Object {$_ -ne $family})
    foreach($entry in $names){$key.DeleteValue($entry)}
    for($i=0;$i -lt $values.Count;$i++){$key.SetValue([string]($i+1),$values[$i],[Microsoft.Win32.RegistryValueKind]::String)}
   }finally{$key.Dispose()}
  }
  $package|Remove-AppxPackage
 }
 Write-Host 'Preview identity removed. The installed OpenRGB identity and certificate are unchanged.';return
}
$AppRoot=[IO.Path]::GetFullPath($AppRoot)
$package=Join-Path $AppRoot 'OpenRGB.LightSync.Preview.msix'
if(-not(Test-Path -LiteralPath $package)){throw 'Preview identity package missing.'}
$signature=Get-AuthenticodeSignature -LiteralPath (Join-Path $AppRoot 'OpenRGB.exe')
if($signature.Status -ne 'Valid'){throw 'The preview executable must have a valid signature.'}
$backup=Join-Path $AppRoot ('identity-backup-'+(Get-Date -Format yyyyMMdd-HHmmss)+'.reg')
& reg.exe export 'HKCU\Software\Microsoft\Lighting' $backup /y | Out-Null
if($LASTEXITCODE -ne 0){throw 'Cannot back up Windows Lighting settings.'}
Get-AppxPackage -Name $name | Remove-AppxPackage
Add-AppxPackage -Path $package -ExternalLocation $AppRoot
if($PrioritizeKeyboard){
 $family=(Get-AppxPackage -Name $name).PackageFamilyName
 $keys=@('Software\Microsoft\Lighting\Providers')
 foreach($device in Get-ChildItem 'HKCU:\Software\Microsoft\Lighting\Devices' -ErrorAction SilentlyContinue){
  if($device.PSChildName -match 'VID_048D&PID_C193'){$keys+=($device.Name.Substring('HKEY_CURRENT_USER\'.Length)+'\Providers')}
 }
 foreach($path in $keys){
  $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($path,$true);if(-not $key){continue}
  try {
   $names=@($key.GetValueNames()|Where-Object {$_ -match '^\d+$'}|Sort-Object {[int]$_})
   $values=@($names|ForEach-Object {[string]$key.GetValue($_)})
   $ordered=@($family)+@($values|Where-Object {$_ -ne $family})
   foreach($entry in $names){$key.DeleteValue($entry)}
   for($i=0;$i -lt $ordered.Count;$i++){$key.SetValue([string]($i+1),$ordered[$i],[Microsoft.Win32.RegistryValueKind]::String)}
  }finally{$key.Dispose()}
 }
}
Write-Host "Preview registered. Lighting settings backup: $backup"
