@echo off
call "%~dp0docker-test-hello-world.bat" arm64-native
exit /b %errorlevel%
