@echo off
call "%~dp0docker-test-hello-world-cpp.bat" x64-native
exit /b %errorlevel%
