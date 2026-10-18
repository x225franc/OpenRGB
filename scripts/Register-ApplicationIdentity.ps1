param(
    [string]$AppRoot=(Join-Path $PSScriptRoot '..\app'),
    [string]$BackupRoot=(Join-Path $PSScriptRoot '..\app\identity-backups'),
    [switch]$RebindOnly,
    [switch]$Remove,
    [switch]$PrioritizeKeyboard
)
$ErrorActionPreference='Stop'
$name='OpenRGB.LightSync'
if(Get-Process OpenRGB -ErrorAction SilentlyContinue){throw 'Quit OpenRGB before changing its Windows identity registration.'}
$AppRoot=[IO.Path]::GetFullPath($AppRoot)
$stamp=Get-Date -Format yyyyMMdd-HHmmss
$backup=Join-Path ([IO.Path]::GetFullPath($BackupRoot)) $stamp
New-Item -ItemType Directory -Path $backup -Force|Out-Null
if(Test-Path 'HKCU:\Software\Microsoft\Lighting'){
    & reg.exe export 'HKCU\Software\Microsoft\Lighting' (Join-Path $backup 'Lighting.reg') /y|Out-Null
    if($LASTEXITCODE -ne 0){throw 'Cannot back up Windows Lighting settings.'}
}
$previous=Get-AppxPackage -Name $name
$legacy=Get-AppxPackage -Name OpenRGB.LightSync.Preview
$previous|Select-Object Name,PackageFullName,PackageFamilyName,Version,InstallLocation|ConvertTo-Json|Set-Content (Join-Path $backup 'previous-package.json') -Encoding utf8
$paths=@('Software\Microsoft\Lighting\Providers')
foreach($device in Get-ChildItem 'HKCU:\Software\Microsoft\Lighting\Devices' -ErrorAction SilentlyContinue){$paths+=($device.Name.Substring('HKEY_CURRENT_USER\'.Length)+'\Providers')}
$providers=@{}
foreach($path in $paths){
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($path)
    if(-not $key){continue}
    try {
        $values=@($key.GetValueNames()|Where-Object {$_ -match '^\d+$'}|Sort-Object {[int]$_}|ForEach-Object {[string]$key.GetValue($_)})
        if($legacy){$values=@($values|Where-Object {$_ -notlike ($legacy.PackageFamilyName+'*')})}
        $providers[$path]=$values
    }finally{$key.Dispose()}
}
$providers|ConvertTo-Json -Depth 5|Set-Content (Join-Path $backup 'providers.json') -Encoding utf8
if($Remove){
    if($previous){$previous|Remove-AppxPackage}
    Write-Host "OpenRGB identity removed. Backup: $backup";return
}
$exe=Join-Path $AppRoot 'OpenRGB.exe'
if((Get-AuthenticodeSignature -LiteralPath $exe).Status -ne 'Valid'){throw 'The executable must have a valid signature.'}
if($RebindOnly -and -not $previous){throw 'No existing OpenRGB identity is available. Register the signed package first.'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$identityDirectory=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\app'))
$candidates=@()
foreach($filename in @('OpenRGB.LightSync.msix','OpenRGB.LightSync.Switch.msix')){
    $candidate=Join-Path $identityDirectory $filename
    if(-not(Test-Path -LiteralPath $candidate)){continue}
    $archive=[IO.Compression.ZipFile]::OpenRead($candidate)
    try {
        $entry=$archive.GetEntry('AppxManifest.xml')
        if(-not $entry){throw 'Identity package manifest missing.'}
        $reader=New-Object IO.StreamReader($entry.Open())
        try{[xml]$xml=$reader.ReadToEnd()}finally{$reader.Dispose()}
        if($xml.Package.Identity.Name -ne $name -or $xml.Package.Identity.Publisher -ne 'CN=OpenRGBLightSync'){throw 'Unexpected identity package.'}
        $candidates+=@{Path=$candidate;Version=[Version]$xml.Package.Identity.Version}
    }finally{$archive.Dispose()}
}
# A version change permits redirecting the external folder. ForceUpdateFromAnyVersion
# also permits using the first signed revision again when switching back.
$selected=@($candidates|Where-Object {-not $previous -or $_.Version -ne [Version]$previous.Version}|Select-Object -First 1)
if(-not $selected.Count){throw 'A differently versioned signed identity package is required to retain settings while changing folders.'}
Add-AppxPackage -Path $selected[0].Path -ExternalLocation $AppRoot -ForceUpdateFromAnyVersion
$current=Get-AppxPackage -Name $name
if(-not $current -or ($previous -and $current.PackageFamilyName -ne $previous.PackageFamilyName)){throw 'The original OpenRGB package family was not preserved.'}
if($legacy){$legacy|Remove-AppxPackage}
if($previous){
    # Preserve the previous provider order, excluding only the temporary preview entry.
    foreach($path in $providers.Keys){
        $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($path)
        try {
            foreach($entry in @($key.GetValueNames()|Where-Object {$_ -match '^\d+$'})){$key.DeleteValue($entry)}
            $values=@($providers[$path])
            for($i=0;$i -lt $values.Count;$i++){$key.SetValue([string]($i+1),[string]$values[$i],[Microsoft.Win32.RegistryValueKind]::String)}
        }finally{$key.Dispose()}
    }
}
if($PrioritizeKeyboard){
    $priorityPaths=@('Software\Microsoft\Lighting\Providers')
    foreach($device in Get-ChildItem 'HKCU:\Software\Microsoft\Lighting\Devices' -ErrorAction SilentlyContinue){if($device.PSChildName -match 'VID_048D&PID_C193'){$priorityPaths+=($device.Name.Substring('HKEY_CURRENT_USER\'.Length)+'\Providers')}}
    foreach($path in $priorityPaths){
        $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($path)
        try {
            $names=@($key.GetValueNames()|Where-Object {$_ -match '^\d+$'}|Sort-Object {[int]$_})
            $values=@($names|ForEach-Object {[string]$key.GetValue($_)}|Where-Object {$_ -ne $current.PackageFamilyName})
            $values=@($current.PackageFamilyName)+$values
            foreach($entry in $names){$key.DeleteValue($entry)}
            for($i=0;$i -lt $values.Count;$i++){$key.SetValue([string]($i+1),$values[$i],[Microsoft.Win32.RegistryValueKind]::String)}
        }finally{$key.Dispose()}
    }
}
[ordered]@{Name=$current.Name;Family=$current.PackageFamilyName;Version=$current.Version.ToString();AppRoot=$AppRoot;Backup=$backup;LegacyPreviewRemoved=[bool]$legacy}|ConvertTo-Json|Set-Content (Join-Path $backup 'registration.json') -Encoding utf8
Write-Host "OpenRGB identity preserved: $($current.PackageFamilyName). Backup: $backup"