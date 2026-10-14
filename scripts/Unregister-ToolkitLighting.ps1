param([string]$ToolkitRoot='C:\Program Files\LenovoLegionToolkit',[switch]$Restore,[string]$BackupDirectory="$env:LOCALAPPDATA\OpenRGB-LightSync\ToolkitIdentityBackup")
$ErrorActionPreference='Stop'
$name='eef45acd-2cf3-4d7d-9d33-92f37c74cc31'
if($Restore){
 Add-AppxPackage -Path (Join-Path $ToolkitRoot 'LenovoLegionToolkit.LampArray.msix') -ExternalLocation $ToolkitRoot
 Write-Output 'Toolkit Dynamic Lighting identity restored. Adjust lighting priority in Windows if desired.';return
}
$package=Get-AppxPackage -Name $name
if(-not $package){Write-Output 'Toolkit lighting identity already unregistered.';return}
New-Item -ItemType Directory -Path $BackupDirectory -Force | Out-Null
$backup=Join-Path $BackupDirectory ('Lighting-'+(Get-Date -Format yyyyMMdd-HHmmss)+'.reg')
& reg.exe export 'HKCU\Software\Microsoft\Lighting' $backup /y | Out-Null
if($LASTEXITCODE -ne 0){throw 'Lighting registry backup failed.'}
$package | Select-Object Name,PackageFullName,PackageFamilyName,InstallLocation | ConvertTo-Json | Set-Content (Join-Path $BackupDirectory 'ToolkitIdentity.json')
$family=$package.PackageFamilyName
Remove-AppxPackage -Package $package.PackageFullName
$keys=@('HKCU:\Software\Microsoft\Lighting\Providers')+@(Get-ChildItem 'HKCU:\Software\Microsoft\Lighting\Devices' | ForEach-Object { $_.PSPath+'\Providers' })
foreach($path in $keys){
 if(-not(Test-Path $path)){continue}
 $key=Get-Item $path
 try{
  $names=@($key.GetValueNames() | Where-Object {$_ -match '^\d+$'} | Sort-Object {[int]$_})
  $values=@($names | ForEach-Object {[string]$key.GetValue($_)})
  if($values -notcontains $family){continue}
  $ordered=@($values | Where-Object {$_ -ne $family})
  foreach($item in $names){$key.DeleteValue($item)}
  for($i=0;$i -lt $ordered.Count;$i++){$key.SetValue([string]($i+1),$ordered[$i],[Microsoft.Win32.RegistryValueKind]::String)}
 }finally{$key.Dispose()}
}
Write-Output "Toolkit Dynamic Lighting identity removed; executable and external settings retained. Registry backup: $backup"
Write-Output 'Undo: Unregister-ToolkitLighting.ps1 -Restore'
