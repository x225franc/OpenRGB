@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d "%~dp0..\..\build"
cl /nologo /EHsc /std:c++17 /utf-8 /MD /DHID_API_NO_EXPORT_DEFINE /I..\tests\RazerSupport\stub /I..\Controllers\RazerController /I..\Controllers\RazerController\RazerController /I..\dependencies\hidapi-hotplug-win\include /I..\interop /I..\KeyboardLayoutManager /I..\RGBController /I..\dependencies\json /I.. /Fe:RazerTransportTests.exe ..\tests\RazerSupport\RazerTransportTests.cpp ..\Controllers\RazerController\RazerController\RazerController.cpp ..\Controllers\RazerController\RazerDevices.cpp ..\Controllers\RazerController\RazerDeviceGuard.cpp ..\interop\DeviceGuard.cpp ..\interop\DeviceGuardLock.cpp ..\interop\DeviceGuardManager.cpp ..\RGBController\RGBControllerKeyNames.cpp advapi32.lib
if errorlevel 1 exit /b 1
RazerTransportTests.exe
