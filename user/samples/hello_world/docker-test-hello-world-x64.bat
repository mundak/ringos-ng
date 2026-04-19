@echo off
call "%~dp0docker-test-hello-world.bat" x64-native
exit /b %errorlevel%
