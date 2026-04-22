@echo off
call "%~dp0docker-test-console-service-write.bat" arm64-x64-emulator
exit /b %errorlevel%
