@echo off
call "%~dp0docker-test-hello-world-cpp.bat" arm64-native
exit /b %errorlevel%
