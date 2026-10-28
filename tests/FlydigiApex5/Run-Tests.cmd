@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1
cd /d "%~dp0..\..\build"
cl /nologo /EHsc /std:c++17 /W4 /MD /Fe:FlydigiProtocolTests.exe ..\tests\FlydigiApex5\ProtocolTests.cpp
if errorlevel 1 exit /b 1
FlydigiProtocolTests.exe
