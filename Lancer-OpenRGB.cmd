@echo off
setlocal
cd /d "%~dp0app"
if not exist "OpenRGB.exe" (echo OpenRGB.exe is missing in the app directory.& exit /b 1)
start "" "OpenRGB.exe" %*