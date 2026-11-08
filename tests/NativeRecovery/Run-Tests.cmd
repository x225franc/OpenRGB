@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
set "QTROOT=C:\Laragon\www\OpenRGB\Plugin\build-tools\qt\msvc2022_64"
set "PATH=%QTROOT%\bin;%PATH%"
cd /d C:\Laragon\www\OpenRGB\new_version
cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /W4 /MD /Febuild\native-recovery\RecoveryTests.exe /Fobuild\native-recovery\RecoveryTests.obj tests\NativeRecovery\RecoveryTests.cpp /link user32.lib shell32.lib
if errorlevel 1 exit /b 1
build\native-recovery\RecoveryTests.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /W4 /MD /DQT_CORE_LIB /DQT_NO_DEBUG /I"%QTROOT%\include" /I"%QTROOT%\include\QtCore" /Febuild\native-recovery\PowerEventTests.exe /Fobuild\native-recovery\ tests\NativeRecovery\PowerEventTests.cpp /link /LIBPATH:"%QTROOT%\lib" Qt6Core.lib user32.lib shell32.lib
if errorlevel 1 exit /b 1
build\native-recovery\PowerEventTests.exe
exit /b %errorlevel%
