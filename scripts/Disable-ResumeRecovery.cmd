@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Configure-ResumeRecovery.ps1" -Remove
pause
