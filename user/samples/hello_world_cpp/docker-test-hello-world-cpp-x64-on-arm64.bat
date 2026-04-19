@echo off
call "%~dp0docker-test-hello-world-cpp.bat" arm64-x64-emulator
exit /b %errorlevel%
