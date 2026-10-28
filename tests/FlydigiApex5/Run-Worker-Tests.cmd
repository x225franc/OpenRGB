@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1
cd /d "%~dp0..\.."
python tests\FlydigiApex5\GenerateWorkerTest.py build\flydigi-worker-test.cpp
if errorlevel 1 exit /b 1
cd build
cl /nologo /EHsc /std:c++17 /W4 /MD /Fe:FlydigiWorkerTests.exe flydigi-worker-test.cpp
if errorlevel 1 exit /b 1
FlydigiWorkerTests.exe
