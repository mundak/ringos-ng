@echo off
setlocal EnableExtensions

echo === Running console_service_write sample test: x64 ===
call "%~dp0..\..\..\tests\docker-run-sample-test.bat" "user/samples/console_service_write/test-console-service-write.sh x64"
if errorlevel 1 (
    exit /b %errorlevel%
)

echo === Running console_service_write sample test: arm64 ===
call "%~dp0..\..\..\tests\docker-run-sample-test.bat" "user/samples/console_service_write/test-console-service-write.sh arm64"
if errorlevel 1 (
    exit /b %errorlevel%
)

echo === Running console_service_write sample test: x64-on-arm64 ===
call "%~dp0..\..\..\tests\docker-run-sample-test.bat" "user/samples/console_service_write/test-console-service-write.sh x64-on-arm64"
exit /b %errorlevel%
