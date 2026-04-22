@echo off
call "%~dp0docker-test-console-service-write.bat" arm64-native
exit /b %errorlevel%
