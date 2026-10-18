param(
    [string]$Destination=(Join-Path $env:USERPROFILE 'Downloads'),
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{40}$')][string]$SourceCommit
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$app=Join-Path $root 'app'
if(Get-Process OpenRGB -ErrorAction SilentlyContinue){throw 'Close OpenRGB before backing up its configuration.'}
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
    & robocopy.exe $source (Join-Path $stage ('app\'+$folder)) /E /XD logs /XF OpenRGBUiReview.dll *.log *.dmp *.tmp /R:1 /W:1 /NFL /NDL /NJH /NJS /NP|Out-Null
    if($LASTEXITCODE -gt 7){throw "Copy failed: $folder"}
}
$backups=@(Get-ChildItem -LiteralPath $app -Filter 'identity-backup-*.reg' -File)
if($backups.Count){New-Item -ItemType Directory -Path (Join-Path $stage 'app\identity-backups') -Force|Out-Null;$backups|Copy-Item -Destination (Join-Path $stage 'app\identity-backups')}
foreach($file in @('LISEZ-MOI.md','Lancer-OpenRGB.cmd','VALIDATION-MODERN.md','LICENSE')){Copy-Item -LiteralPath (Join-Path $root $file) -Destination $stage}
Copy-Item -LiteralPath (Join-Path $root 'Documentation\MODERN-INTERFACE.md') -Destination (Join-Path $stage 'Documentation')
foreach($file in @('Register-ApplicationIdentity.ps1','Prepare-ApplicationIdentity.ps1')){Copy-Item -LiteralPath (Join-Path $root ('scripts\'+$file)) -Destination (Join-Path $stage 'scripts')}
Copy-Item -LiteralPath (Join-Path $root 'qt\OpenRGBMonochrome.png') -Destination (Join-Path $stage 'qt')
$configuration=Join-Path $stage 'app\config\OpenRGB.json'
# Preserve case-sensitive device keys when editing this JSON.
# Do not round-trip it through ConvertFrom-Json in Windows PowerShell.
if((Get-Content -LiteralPath $configuration -Raw -Encoding utf8) -match 'OpenRGB UI Review'){throw 'Remove the development plugin settings before packaging.'}
$privateFiles=@(Get-ChildItem -LiteralPath $stage -Recurse -File|Where-Object {$_.Extension -in @('.pfx','.p12','.key') -or $_.Name -eq 'OpenRGBUiReview.dll'})
if($privateFiles.Count){throw 'The payload contains a private key or development driver.'}
$plugins=@(Get-ChildItem -LiteralPath (Join-Path $stage 'app\plugins') -Filter '*.dll' -File)
if($plugins.Count -ne 3){throw 'Expected exactly Effects, LightSync and Visual Map.'}
$hashes=@(Get-ChildItem -LiteralPath (Join-Path $stage 'app') -Recurse -File|Where-Object {$_.Extension -in @('.exe','.dll','.msix','.cer')}|ForEach-Object {@{Path=$_.FullName.Substring($stage.Length+1).Replace('\','/');SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})
@{Created=(Get-Date).ToUniversalTime().ToString('o');SourceCommit=$SourceCommit;SourceBranch='feature/fluent-interface';SourceRepository='https://github.com/x225franc/OpenRGB';Configuration='Release x64';Qt='6.8.3';PluginAPI=5;ApplicationIdentity='OpenRGB.LightSync';ExecutableSignature=$signature.Status.ToString();Files=$hashes}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $stage 'BuildManifest.json') -Encoding utf8
$Destination=[IO.Path]::GetFullPath($Destination)
if(-not(Test-Path -LiteralPath $Destination)){throw 'The destination directory must already exist.'}
$zip=Join-Path $Destination ($name+'.zip')
if(Test-Path -LiteralPath $zip){throw 'The destination ZIP already exists.'}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
$result=@{Archive=$zip;Stage=$stage;SHA256=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash;Bytes=(Get-Item -LiteralPath $zip).Length;SourceCommit=$SourceCommit}
$result|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root 'build\package-result.json') -Encoding utf8
$result