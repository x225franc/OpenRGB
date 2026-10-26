@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1
if not exist "%~dp0..\build" mkdir "%~dp0..\build"
cd /d "%~dp0..\build"
cl /nologo /EHsc /std:c++17 /O2 /MD /I..\RGBController /I..\dependencies\json ..\tests\LightingModes\LightingModesTests.cpp /Fe:lighting-modes-tests.exe
if errorlevel 1 exit /b 1
lighting-modes-tests.exe
exit /b %errorlevel%
