@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1
set "PATH=C:\Laragon\www\OpenRGB\Plugin\build-tools\qt\msvc2022_64\bin;C:\Laragon\www\OpenRGB\Plugin\build-tools\jom;%PATH%"
cd /d "%~dp0"
if not exist build mkdir build
cd build
qmake ..\OpenRGBLightSyncPlugin.pro "CONFIG+=release"
if errorlevel 1 exit /b 1
jom -j 4
exit /b %errorlevel%
