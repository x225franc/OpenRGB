@echo off
setlocal
if not defined QT_ROOT set "QT_ROOT=%~dp0..\..\Plugin\build-tools\qt\msvc2022_64"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1
set "PATH=%QT_ROOT%\bin;%~dp0..\..\Plugin\build-tools\jom;%PATH%"
set "QT_QPA_PLATFORM=offscreen"
set "QT_QPA_FONTDIR=%WINDIR%\Fonts"
if not exist "%~dp0..\build\modern-tests" mkdir "%~dp0..\build\modern-tests"
cd /d "%~dp0..\build\modern-tests"
qmake ..\..\tests\modern-interface\ModernNavigationTests.pro "CONFIG+=release"
if errorlevel 1 exit /b 1
jom -j 4
if errorlevel 1 exit /b 1
ModernNavigationTests.exe -o results.txt,txt
exit /b %errorlevel%
