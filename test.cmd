@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
set "PATH=C:\Laragon\www\OpenRGB\Plugin\build-tools\qt\msvc2022_64\bin;C:\Laragon\www\OpenRGB\Plugin\build-tools\jom;%PATH%"
cd /d "%~dp0build"
if not exist tests mkdir tests
cd tests
qmake ..\..\ProtocolTests.pro "CONFIG+=release"
if errorlevel 1 exit /b 1
jom -j 4
if errorlevel 1 exit /b 1
LightSyncProtocolTests.exe ..\OpenRGBLightSyncPlugin.dll
exit /b %errorlevel%
