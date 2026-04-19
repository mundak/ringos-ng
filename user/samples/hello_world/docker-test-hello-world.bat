@echo off
setlocal EnableExtensions

if "%~1"=="" (
    echo Usage: user\samples\hello_world\docker-test-hello-world.bat ^<x64-native^|arm64-native^|arm64-x64-emulator^> [docker-image-name]
    exit /b 1
)

call "%~dp0..\..\..\tests\docker-run-sample-test.bat" "user/samples/hello_world/test-hello-world.sh %~1" "%~2"
exit /b %errorlevel%
