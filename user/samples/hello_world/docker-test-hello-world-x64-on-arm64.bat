@echo off
call "%~dp0docker-test-hello-world.bat" arm64-x64-emulator
exit /b %errorlevel%
