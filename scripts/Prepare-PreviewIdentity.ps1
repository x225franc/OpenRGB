param([string]$InstalledRoot=(Join-Path $PSScriptRoot '..\app'))
$ErrorActionPreference='Stop'
$buildRoot=Join-Path $PSScriptRoot '..\build\identity'
$layout=Join-Path $buildRoot 'layout'
New-Item -ItemType Directory -Path $buildRoot,(Join-Path $layout 'Images'),(Join-Path $layout 'public') -Force | Out-Null
$sdk='C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64'
$certificate=Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq 'CN=OpenRGBLightSync' -and $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date).AddDays(30) } | Select-Object -First 1
if(-not $certificate){throw 'The existing OpenRGBLightSync signing certificate is not available for this Windows user. Run under the desktop account.'}
Export-Certificate -Cert $certificate -FilePath (Join-Path $buildRoot 'OpenRGBLightSync.cer') | Out-Null
$manifest='<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10" xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10" xmlns:uap3="http://schemas.microsoft.com/appx/manifest/uap/windows10/3" xmlns:uap10="http://schemas.microsoft.com/appx/manifest/uap/windows10/10" xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities" IgnorableNamespaces="uap uap3 uap10 rescap">
 <Identity Name="OpenRGB.LightSync.Preview" Publisher="CN=OpenRGBLightSync" Version="1.0.0.0" ProcessorArchitecture="x64"/>
 <Properties><DisplayName>OpenRGB Preview</DisplayName><PublisherDisplayName>OpenRGB Preview local build</PublisherDisplayName><Logo>Images\StoreLogo.png</Logo><uap10:AllowExternalContent>true</uap10:AllowExternalContent></Properties>
 <Dependencies><TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.19041.0" MaxVersionTested="10.0.26100.0"/></Dependencies>
 <Resources><Resource Language="en-US"/></Resources>
 <Applications><Application Id="App" Executable="OpenRGB.exe" uap10:TrustLevel="mediumIL" uap10:RuntimeBehavior="win32App">
 <uap:VisualElements DisplayName="OpenRGB Preview" Description="OpenRGB Windows Dynamic Lighting controller" BackgroundColor="transparent" Square150x150Logo="Images\Square150x150Logo.png" Square44x44Logo="Images\Square44x44Logo.png" AppListEntry="none"/>
 <Extensions><uap3:Extension Category="windows.appExtension"><uap3:AppExtension Name="com.microsoft.windows.lighting" Id="OpenRGBLighting" PublicFolder="public" DisplayName="OpenRGB Preview"/></uap3:Extension></Extensions>
 </Application></Applications>
 <Capabilities><Capability Name="internetClient"/><rescap:Capability Name="runFullTrust"/><DeviceCapability Name="humaninterfacedevice"/></Capabilities>
</Package>'
Set-Content -LiteralPath (Join-Path $layout 'AppxManifest.xml') -Value $manifest -Encoding utf8
Add-Type -AssemblyName System.Drawing
$logo=[Drawing.Image]::FromFile((Join-Path $PSScriptRoot '..\qt\org.openrgb.OpenRGB.png'))
try {
 foreach($entry in @(@{Name='StoreLogo';Size=50},@{Name='Square150x150Logo';Size=150},@{Name='Square44x44Logo';Size=44})){
  $bitmap=New-Object Drawing.Bitmap($entry.Size,$entry.Size)
  $graphics=[Drawing.Graphics]::FromImage($bitmap)
  try{$graphics.Clear([Drawing.Color]::Transparent);$graphics.DrawImage($logo,0,0,$entry.Size,$entry.Size);$bitmap.Save((Join-Path $layout ('Images\'+$entry.Name+'.png')),[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bitmap.Dispose()}
 }
}finally{$logo.Dispose()}
$exe=Join-Path $buildRoot 'OpenRGB.exe'
Copy-Item -LiteralPath (Join-Path $InstalledRoot 'OpenRGB.exe') -Destination $exe -Force
$win32Manifest=Join-Path $buildRoot 'OpenRGB.manifest'
& (Join-Path $sdk 'mt.exe') '-nologo' "-inputresource:$exe;#1" "-out:$win32Manifest"
if($LASTEXITCODE -ne 0){throw 'Extracting OpenRGB application manifest failed'}
[xml]$xml=Get-Content -LiteralPath $win32Manifest
$existing=$xml.SelectSingleNode("//*[local-name()='msix']")
if($existing){$existing.ParentNode.RemoveChild($existing) | Out-Null}
$identity=$xml.CreateElement('msix','urn:schemas-microsoft-com:msix.v1')
$identity.SetAttribute('publisher','CN=OpenRGBLightSync');$identity.SetAttribute('packageName','OpenRGB.LightSync.Preview');$identity.SetAttribute('applicationId','App')
$xml.DocumentElement.AppendChild($identity) | Out-Null
$xml.Save($win32Manifest)
& (Join-Path $sdk 'mt.exe') '-nologo' '-manifest' $win32Manifest "-outputresource:$exe;#1"
if($LASTEXITCODE -ne 0){throw 'Embedding OpenRGB application identity failed'}
& (Join-Path $sdk 'signtool.exe') sign /fd SHA256 /sha1 $certificate.Thumbprint $exe
if($LASTEXITCODE -ne 0){throw 'Signing OpenRGB identity executable failed'}
$package=Join-Path $buildRoot 'OpenRGB.LightSync.Preview.msix'
& (Join-Path $sdk 'makeappx.exe') pack /d $layout /p $package /o /nv
if($LASTEXITCODE -ne 0){throw 'Packaging OpenRGB identity failed'}
& (Join-Path $sdk 'signtool.exe') sign /fd SHA256 /sha1 $certificate.Thumbprint $package
if($LASTEXITCODE -ne 0){throw 'Signing OpenRGB identity package failed'}
[ordered]@{Exe=$exe;Package=$package;Certificate=(Join-Path $buildRoot 'OpenRGBLightSync.cer');Thumbprint=$certificate.Thumbprint;ExeHash=(Get-FileHash $exe).Hash;PackageHash=(Get-FileHash $package).Hash} | ConvertTo-Json | Set-Content (Join-Path $buildRoot 'prepared.json')
