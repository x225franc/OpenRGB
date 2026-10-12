param([string]$Target='C:\Program Files\OpenRGB')
$ErrorActionPreference='Stop'
if(Get-Process OpenRGB -ErrorAction SilentlyContinue){throw 'Quittez OpenRGB avant installation (enregistrez profil et VMap).'}
$payload=Join-Path $PSScriptRoot 'payload'
foreach($file in 'OpenRGB.exe','Qt6Network.dll','plugins\OpenRGBLightSyncPlugin.dll','plugins\OpenRGBEffectsPlugin.dll','plugins\OpenRGBVisualMapPlugin.dll','OpenRGB.LightSync.msix','OpenRGBLightSync.cer'){if(-not(Test-Path (Join-Path $payload $file))){throw "Fichier absent : $file"}}
$backup=Join-Path $Target ('Backup-installation\LightSync-'+(Get-Date -Format yyyyMMdd-HHmmss))
New-Item -ItemType Directory -Path $backup,(Join-Path $Target 'plugins') -Force | Out-Null
$files=@('OpenRGB.exe','Qt6Network.dll','plugins\OpenRGBLightSyncPlugin.dll','plugins\OpenRGBEffectsPlugin.dll','plugins\OpenRGBVisualMapPlugin.dll')
$existing=@()
foreach($file in $files){$path=Join-Path $Target $file;if(Test-Path $path){$existing+=$file;$dest=Join-Path $backup $file;New-Item -ItemType Directory -Path (Split-Path $dest) -Force | Out-Null;Copy-Item -LiteralPath $path -Destination $dest}}
if(Test-Path (Join-Path $Target 'config')){Copy-Item -LiteralPath (Join-Path $Target 'config') -Destination $backup -Recurse}
$cert=New-Object System.Security.Cryptography.X509Certificates.X509Certificate2((Join-Path $payload 'OpenRGBLightSync.cer'))
$wasTrusted=Test-Path ('Cert:\LocalMachine\TrustedPeople\'+$cert.Thumbprint)
$previous=Get-AppxPackage -Name OpenRGB.LightSync
$previous | Select-Object Name,PackageFullName,InstallLocation | ConvertTo-Json | Set-Content (Join-Path $backup 'previous-package.json')
@{Target=$Target;Files=$files;Existing=$existing;Thumbprint=$cert.Thumbprint;CertificateAlreadyTrusted=$wasTrusted} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $backup 'rollback.json')
Import-Certificate -FilePath (Join-Path $payload 'OpenRGBLightSync.cer') -CertStoreLocation Cert:\LocalMachine\TrustedPeople | Out-Null
foreach($file in $files){Copy-Item -LiteralPath (Join-Path $payload $file) -Destination (Join-Path $Target $file) -Force}
if($previous){$previous | Remove-AppxPackage}
Add-AppxPackage -Path (Join-Path $payload 'OpenRGB.LightSync.msix') -ExternalLocation $Target
Write-Host "Installe. Sauvegarde : $backup"
Write-Host 'Lancez OpenRGB.exe. Parametres Windows > Personnalisation > Eclairage dynamique : placez OpenRGB LightSync en premier.'
