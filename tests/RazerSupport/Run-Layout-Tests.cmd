@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d "%~dp0..\..\build"
cl /nologo /EHsc /std:c++17 /utf-8 /MD /I..\tests\RazerSupport\stub /I..\Controllers\RazerController /I..\Controllers\RazerController\RazerController /I..\dependencies\hidapi-hotplug-win\include /I..\interop /I..\KeyboardLayoutManager /I..\RGBController /I..\dependencies\json /I.. /Fe:RazerLayoutTests.exe ..\tests\RazerSupport\RazerLayoutTests.cpp ..\KeyboardLayoutManager\KeyboardLayoutManager.cpp ..\Controllers\RazerController\RazerDevices.cpp ..\RGBController\RGBControllerKeyNames.cpp
if errorlevel 1 exit /b 1
RazerLayoutTests.exe
