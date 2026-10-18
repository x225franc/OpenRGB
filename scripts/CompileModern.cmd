@echo off
setlocal
if not defined QT_ROOT set "QT_ROOT=%~dp0..\..\Plugin\build-tools\qt\msvc2022_64"
if not exist "%QT_ROOT%\bin\qmake.exe" (echo Set QT_ROOT to the Qt 6 MSVC x64 directory.& exit /b 1)
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1
set "PATH=%QT_ROOT%\bin;%~dp0..\..\Plugin\build-tools\jom;%PATH%"
if not exist "%~dp0..\build" mkdir "%~dp0..\build"
cd /d "%~dp0..\build"
qmake ..\OpenRGB.pro "CONFIG-=debug_and_release" "CONFIG+=release"
if errorlevel 1 exit /b 1
jom -j 6
exit /b %errorlevel%
