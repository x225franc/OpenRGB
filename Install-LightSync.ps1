param([string]$Target='C:\Program Files\OpenRGB')
$ErrorActionPreference='Stop'
if(Get-Process OpenRGB -ErrorAction SilentlyContinue){throw 'Quittez OpenRGB avant installation.'}
$Target=[IO.Path]::GetFullPath($Target)
$payload=Join-Path $PSScriptRoot 'payload'
if(-not(Test-Path $payload)){$payload=Join-Path $PSScriptRoot 'app'}
if((Get-AuthenticodeSignature (Join-Path $payload 'OpenRGB.exe')).Status -ne 'Valid'){throw 'Signature OpenRGB invalide.'}
$backup=Join-Path $Target ('Backup-installation\Modern-'+(Get-Date -Format yyyyMMdd-HHmmss))
New-Item -ItemType Directory -Path $backup -Force|Out-Null
$files=@(Get-ChildItem $payload -Recurse -File|Where-Object {$_.FullName.Substring($payload.Length+1) -notmatch '^(config|identity-backups)\\'})
$existing=@()
foreach($file in $files){$relative=$file.FullName.Substring($payload.Length+1);$dest=Join-Path $Target $relative;if(Test-Path -LiteralPath $dest){$existing+=$relative;$saved=Join-Path $backup $relative;New-Item -ItemType Directory -Path (Split-Path $saved) -Force|Out-Null;Copy-Item -LiteralPath $dest -Destination $saved}}
if(Test-Path (Join-Path $Target 'config')){Copy-Item -LiteralPath (Join-Path $Target 'config') -Destination $backup -Recurse}
@{Target=$Target;Existing=$existing;Files=@($files|ForEach-Object {$_.FullName.Substring($payload.Length+1)})}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $backup 'rollback-modern.json') -Encoding utf8
foreach($file in $files){$dest=Join-Path $Target ($file.FullName.Substring($payload.Length+1));New-Item -ItemType Directory -Path (Split-Path $dest) -Force|Out-Null;Copy-Item -LiteralPath $file.FullName -Destination $dest -Force}
# Keep the installed profiles and LAN choices. Only initialize modern appearance.
$config=Join-Path $Target 'config\OpenRGB.json'
if(Test-Path $config){$text=[IO.File]::ReadAllText($config);if($text -notmatch '"ModernInterface"\s*:'){$text=[regex]::Replace($text,'^\s*\{','{"ModernInterface":{"accent":"windows","material":"acrylic","navigation":"expanded"},',1);[IO.File]::WriteAllText($config,$text,(New-Object Text.UTF8Encoding($false)))}}
$publicCertificate=Join-Path $payload 'OpenRGBLightSync.cer'
Import-Certificate -FilePath $publicCertificate -CertStoreLocation Cert:\CurrentUser\TrustedPeople|Out-Null
& (Join-Path $PSScriptRoot 'scripts\Register-ApplicationIdentity.ps1') -AppRoot $Target -BackupRoot (Join-Path $backup 'identity')
@{Target=$Target;Backup=$backup;SHA256=(Get-FileHash (Join-Path $Target 'OpenRGB.exe')).Hash;Identity=@(Get-AppxPackage -Name OpenRGB.LightSync|Select-Object PackageFamilyName,Version)}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $backup 'deployment.json') -Encoding utf8
Write-Output "Installation terminee. Sauvegarde : $backup"
