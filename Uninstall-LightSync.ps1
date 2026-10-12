param([Parameter(Mandatory=$true)][string]$Backup,[switch]$RestoreConfiguration,[switch]$RemoveCertificate)
$ErrorActionPreference='Stop'
if(Get-Process OpenRGB -ErrorAction SilentlyContinue){throw 'Quittez OpenRGB avant restauration.'}
$state=Get-Content (Join-Path $Backup 'rollback.json') -Raw | ConvertFrom-Json
Get-AppxPackage -Name OpenRGB.LightSync | Remove-AppxPackage
foreach($file in $state.Files){$target=Join-Path $state.Target $file;if($state.Existing -contains $file){Copy-Item -LiteralPath (Join-Path $Backup $file) -Destination $target -Force}else{if(Test-Path $target){Remove-Item -LiteralPath $target}}}
if($RestoreConfiguration){Copy-Item -Path (Join-Path $Backup 'config\*') -Destination (Join-Path $state.Target 'config') -Recurse -Force}
if($RemoveCertificate){$path='Cert:\LocalMachine\TrustedPeople\'+$state.Thumbprint;if(Test-Path $path){Remove-Item -LiteralPath $path}}
Write-Host 'Anciens binaires restaures. Identite OpenRGB.LightSync desenregistree. Identite Toolkit intacte.'
Write-Host 'Pour refaire : relancez Install-LightSync.ps1 depuis le package sauvegarde.'
