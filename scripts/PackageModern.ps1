param(
    [switch]$OfflineSnapshot,
    [string]$LightSyncCommit="",
    [string]$LegacyDelivery=(Join-Path $PSScriptRoot '..\..\build\hibernate-delivery-20261014-933765b'),
    [string]$Destination=(Join-Path $env:USERPROFILE 'Downloads'),
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{40}$')][string]$SourceCommit
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$app=Join-Path $root 'app'
$running=@(Get-Process OpenRGB -ErrorAction SilentlyContinue)
if($running.Count){
    if(-not $OfflineSnapshot){throw 'Close OpenRGB before backing up its configuration.'}
    foreach($process in $running){
        if(-not $process.Path -or [IO.Path]::GetFullPath($process.Path) -eq (Join-Path $app 'OpenRGB.exe')){throw 'Cannot package a running or unverified source application.'}
    }
}

$signature=Get-AuthenticodeSignature -LiteralPath (Join-Path $app 'OpenRGB.exe')
if($signature.Status -ne 'Valid'){throw 'The preview executable must have a valid signature.'}
$name='OpenRGB-Modern-Preview-x64-'+(Get-Date -Format yyyyMMdd-HHmmss)+'-'+$SourceCommit.Substring(0,8)
$stage=Join-Path $root ('build\distribution\'+$name)
if(Test-Path -LiteralPath $stage){throw 'The staging directory already exists.'}
New-Item -ItemType Directory -Path (Join-Path $stage 'app'),(Join-Path $stage 'Documentation'),(Join-Path $stage 'scripts'),(Join-Path $stage 'qt') -Force|Out-Null
Get-ChildItem -LiteralPath $app -File|Where-Object {$_.Extension -in @('.exe','.dll','.bin','.msix','.cer')}|Copy-Item -Destination (Join-Path $stage 'app')
foreach($folder in @('platforms','imageformats','styles','plugins','config','identity-backups')){
    $source=Join-Path $app $folder
    if(-not(Test-Path -LiteralPath $source)){continue}
    & robocopy.exe $source (Join-Path $stage ('app\'+$folder)) /E /XD logs /XF OpenRGBUiReview.dll *.log *.log.* *.dmp *.tmp /R:1 /W:1 /NFL /NDL /NJH /NJS /NP|Out-Null
    if($LASTEXITCODE -gt 7){throw "Copy failed: $folder"}
}
$backups=@(Get-ChildItem -LiteralPath $app -Filter 'identity-backup-*.reg' -File)
if($backups.Count){New-Item -ItemType Directory -Path (Join-Path $stage 'app\identity-backups') -Force|Out-Null;$backups|Copy-Item -Destination (Join-Path $stage 'app\identity-backups')}
foreach($file in @('LISEZ-MOI.md','Lancer-OpenRGB.cmd','VALIDATION-MODERN.md','SECOND-PC-TEST.md','LIGHTSYNC-NETWORK.md','LICENSE')){Copy-Item -LiteralPath (Join-Path $root $file) -Destination $stage}
Copy-Item -LiteralPath (Join-Path $root 'Documentation\MODERN-INTERFACE.md') -Destination (Join-Path $stage 'Documentation')
foreach($file in @('Register-ApplicationIdentity.ps1','Prepare-ApplicationIdentity.ps1')){Copy-Item -LiteralPath (Join-Path $root ('scripts\'+$file)) -Destination (Join-Path $stage 'scripts')}
Copy-Item -LiteralPath (Join-Path $root 'qt\OpenRGBMonochrome.png') -Destination (Join-Path $stage 'qt')
# Preserve historical evidence, never present it as a fresh validation.
if(-not(Test-Path -LiteralPath $LegacyDelivery)){throw 'Historical delivery not found.'}
foreach($folder in @('payload','validation','validation\historical')){New-Item -ItemType Directory -Path (Join-Path $stage $folder) -Force|Out-Null}
& robocopy.exe (Join-Path $stage 'app') (Join-Path $stage 'payload') /E /XD config identity-backups /R:1 /W:1 /NFL /NDL /NJH /NJS /NP|Out-Null
if($LASTEXITCODE -gt 7){throw 'Payload copy failed.'}
foreach($file in @('Install-LightSync.ps1','Uninstall-LightSync.ps1')){Copy-Item -LiteralPath (Join-Path $root $file) -Destination $stage}
foreach($file in @('Configure-KeyboardPriority.ps1','HIBERNATION-REPORT.md','YEELIGHT-MUSIC-MODE.md')){Copy-Item -LiteralPath (Join-Path $LegacyDelivery $file) -Destination $stage}
Copy-Item -Path (Join-Path $LegacyDelivery 'scripts\*') -Destination (Join-Path $stage 'scripts') -Force
New-Item -ItemType Directory -Path (Join-Path $stage 'payload\scripts') -Force|Out-Null
Copy-Item -Path (Join-Path $LegacyDelivery 'scripts\*') -Destination (Join-Path $stage 'payload\scripts') -Force
Copy-Item -Path (Join-Path $LegacyDelivery 'validation\*') -Destination (Join-Path $stage 'validation\historical') -Recurse
Copy-Item -LiteralPath (Join-Path $LegacyDelivery 'BUILDINFO.json') -Destination (Join-Path $stage 'validation\historical\BUILDINFO.json')
foreach($file in @('logo-toggle-result.json','logo-persistence-result.json','rebind-results.json','windows-accent-state.json','windows-system-accent.json','preview-close.json','modern-deployment.json','settings-footer-final-state.json','single-instance-test.json')){Copy-Item -LiteralPath (Join-Path $root ('build\'+$file)) -Destination (Join-Path $stage 'validation')}
foreach($file in @('bluetooth-reconnect-test-results.log','bluetooth-before-results.log','bluetooth-mouse-test.log','mouse-candidate.json','lightsync-device-management-tests.log','lightsync-device-deployment.json')){Copy-Item -LiteralPath (Join-Path $root ('build\'+$file)) -Destination (Join-Path $stage 'validation')}
Copy-Item -LiteralPath (Join-Path $root 'build\modern-tests\results.txt') -Destination (Join-Path $stage 'validation\modern-tests.txt')
Copy-Item -LiteralPath (Join-Path $root 'build\ui-review\exercise-result.json') -Destination (Join-Path $stage 'validation')
Copy-Item -LiteralPath (Join-Path $root 'VALIDATION-MODERN.md') -Destination (Join-Path $stage 'validation\README.md')
$deployment=Get-Content (Join-Path $root 'build\modern-deployment.json') -Raw|ConvertFrom-Json
if(-not(Test-Path -LiteralPath $deployment.Backup)){throw 'Installation rollback backup missing.'}
& robocopy.exe $deployment.Backup (Join-Path $stage 'installation-backup') /E /XD logs /XF *.dmp *.log *.log.* /R:1 /W:1 /NFL /NDL /NJH /NJS /NP|Out-Null
if($LASTEXITCODE -gt 7){throw 'Installation backup copy failed.'}
foreach($file in @('actions-right-gutter-1280.png','actions-right-gutter-1440.png')){Copy-Item -LiteralPath (Join-Path $root ('build\ui-review\screenshots\'+$file)) -Destination (Join-Path $stage 'validation')}
$configuration=Join-Path $stage 'app\config\OpenRGB.json'
# Preserve case-sensitive device keys when editing this JSON.
# Do not round-trip it through ConvertFrom-Json in Windows PowerShell.
if((Get-Content -LiteralPath $configuration -Raw -Encoding utf8) -match 'OpenRGB UI Review'){throw 'Remove the development plugin settings before packaging.'}
$privateFiles=@(Get-ChildItem -LiteralPath $stage -Recurse -File|Where-Object {$_.Extension -in @('.pfx','.p12','.key') -or $_.Name -eq 'OpenRGBUiReview.dll'})
if($privateFiles.Count){throw 'The payload contains a private key or development driver.'}
$plugins=@(Get-ChildItem -LiteralPath (Join-Path $stage 'app\plugins') -Filter '*.dll' -File)
if($plugins.Count -ne 3){throw 'Expected exactly Effects, LightSync and Visual Map.'}
$hashes=@(Get-ChildItem -LiteralPath $stage -Recurse -File|Where-Object {$_.Extension -in @('.exe','.dll','.msix','.cer')}|ForEach-Object {@{Path=$_.FullName.Substring($stage.Length+1).Replace('\','/');SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})
@{Created=(Get-Date).ToUniversalTime().ToString('o');SourceCommit=$SourceCommit;PluginSourceCommits=@{LightSync=$LightSyncCommit};SourceBranch='feature/fluent-interface';SourceRepository='https://github.com/x225franc/OpenRGB';Configuration='Release x64';Qt='6.8.3';PluginAPI=5;ApplicationIdentity='OpenRGB.LightSync';ExecutableSignature=$signature.Status.ToString();Files=$hashes}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $stage 'BuildManifest.json') -Encoding utf8
Copy-Item -LiteralPath (Join-Path $stage 'BuildManifest.json') -Destination (Join-Path $stage 'BUILDINFO.json')
$Destination=[IO.Path]::GetFullPath($Destination)
if(-not(Test-Path -LiteralPath $Destination)){throw 'The destination directory must already exist.'}
$zip=Join-Path $Destination ($name+'.zip')
if(Test-Path -LiteralPath $zip){throw 'The destination ZIP already exists.'}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
$result=@{Archive=$zip;Stage=$stage;SHA256=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash;Bytes=(Get-Item -LiteralPath $zip).Length;SourceCommit=$SourceCommit}
$result|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root 'build\package-result.json') -Encoding utf8
$result