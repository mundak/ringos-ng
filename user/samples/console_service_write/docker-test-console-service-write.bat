@echo off
setlocal EnableExtensions

if "%~1"=="" (
    echo Usage: user\samples\console_service_write\docker-test-console-service-write.bat ^<x64-native^|arm64-native^|arm64-x64-emulator^> [docker-image-name]
    exit /b 1
)

call "%~dp0..\..\..\tests\docker-run-sample-test.bat" "user/samples/console_service_write/test-console-service-write.sh %~1" "%~2"
exit /b %errorlevel%
