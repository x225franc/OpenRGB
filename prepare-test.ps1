$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$runtime=Join-Path $root 'build\runtime'
New-Item -ItemType Directory -Path $runtime,(Join-Path $runtime 'plugins'),(Join-Path $runtime 'test-config') -Force | Out-Null
Get-ChildItem -LiteralPath 'C:\Program Files\OpenRGB' -File | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $runtime -Force}
foreach($folder in @('platforms','styles','imageformats')){if(Test-Path "C:\Program Files\OpenRGB\$folder"){Copy-Item -LiteralPath "C:\Program Files\OpenRGB\$folder" -Destination $runtime -Recurse -Force}}
Copy-Item -LiteralPath (Join-Path $root 'build\OpenRGBLightSyncPlugin.dll') -Destination (Join-Path $runtime 'plugins') -Force
Copy-Item -LiteralPath 'C:\Laragon\www\OpenRGB\Plugin\build-tools\qt\msvc2022_64\bin\Qt6Network.dll' -Destination $runtime -Force
Set-Content -LiteralPath (Join-Path $runtime 'test-config\OpenRGB.json') -Value '{"LightSync":{"enabled":true,"on_on_resume":false,"off_on_suspend":false},"Server":{"server_port":6744,"start_server":true}}' -Encoding utf8
