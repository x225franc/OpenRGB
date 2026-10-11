param(
    [string]$QtRoot = 'C:\laragon\www\OpenRGB\Plugin\build-tools\qt\msvc2022_64',
    [string]$VcVars = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat',
    [string]$Jom = 'C:\laragon\www\OpenRGB\Plugin\build-tools\jom\jom.exe'
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
foreach ($dependency in @((Join-Path $QtRoot 'bin/qmake.exe'), $VcVars, $Jom)) {
    if (-not (Test-Path -LiteralPath $dependency)) { throw "Missing build dependency: $dependency" }
}
$pluginBuild = Join-Path $repoRoot 'build/optimized'
$testsBuild = Join-Path $repoRoot 'build/tests'
New-Item -ItemType Directory -Path $pluginBuild,$testsBuild -Force | Out-Null
$batch = Join-Path $repoRoot 'build/build-ambient.cmd'
@"
@echo off
call "$VcVars" x64
if errorlevel 1 exit /b 1
set "PATH=$QtRoot\bin;%PATH%"
cd /d "$pluginBuild"
qmake "$repoRoot\OpenRGBEffectsPlugin.pro" "CONFIG-=debug_and_release" "CONFIG+=release"
if errorlevel 1 exit /b 1
"$Jom" -j 4
if errorlevel 1 exit /b 1
cd /d "$testsBuild"
qmake "$PSScriptRoot\AmbientTests.pro" "CONFIG-=debug_and_release" "CONFIG+=release"
if errorlevel 1 exit /b 1
"$Jom" -j 4
if errorlevel 1 exit /b 1
"@ | Set-Content -LiteralPath $batch -Encoding ascii
& cmd.exe /d /c "`"$batch`""
if ($LASTEXITCODE -ne 0) { throw "Ambient build failed with exit code $LASTEXITCODE" }
Write-Output "DLL: $pluginBuild\release\OpenRGBEffectsPlugin.dll"
Write-Output "Tests: $testsBuild\AmbientTests.exe"
