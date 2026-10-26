@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    if errorlevel 1 exit /b 1
)
set "RADEON_ROOT=%~dp0.."
if not exist "%RADEON_ROOT%\build\radeon-tests" mkdir "%RADEON_ROOT%\build\radeon-tests"
pushd "%RADEON_ROOT%\build\radeon-tests"
cl /nologo /EHsc /std:c++17 /W4 /Fe:ProtocolTests.exe "%RADEON_ROOT%\tests\RX7900Reference\ProtocolTests.cpp"
if errorlevel 1 goto failed
ProtocolTests.exe "%RADEON_ROOT%\tests\RX7900Reference\RecordedMessages.txt" "%RADEON_ROOT%\tests\RX7900Reference\RecordedFirmwareMessages.txt"
if errorlevel 1 goto failed
cl /nologo /EHsc /std:c++17 /W4 /I"%RADEON_ROOT%\RGBController" /I"%RADEON_ROOT%\dependencies\json" /Fe:OutputTests.exe "%RADEON_ROOT%\tests\RX7900Reference\OutputTests.cpp"
if errorlevel 1 goto failed
OutputTests.exe
if errorlevel 1 goto failed
cl /nologo /EHsc /std:c++17 /W4 /I"%RADEON_ROOT%\dependencies\display-library\include" /Fe:NativeProbe.exe "%RADEON_ROOT%\tests\RX7900Reference\NativeProbe_Windows.cpp"
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
