@echo off
call "%~dp0docker-test-console-service-write.bat" x64-native
exit /b %errorlevel%
