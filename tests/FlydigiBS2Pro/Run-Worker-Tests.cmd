@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1
cd /d "%~dp0..\.."
python tests\FlydigiBS2Pro\GenerateWorkerTest.py build\bs2-worker-test.cpp
if errorlevel 1 exit /b 1
cd build
cl /nologo /EHsc /std:c++17 /W4 /MD /Fe:BS2WorkerTests.exe bs2-worker-test.cpp
if errorlevel 1 exit /b 1
.\BS2WorkerTests.exe
