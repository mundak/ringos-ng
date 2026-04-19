@echo off
setlocal EnableExtensions

if "%~1"=="" (
    echo Usage: user\samples\hello_world_cpp\docker-test-hello-world-cpp.bat ^<x64-native^|arm64-native^|arm64-x64-emulator^> [docker-image-name]
    exit /b 1
)

call "%~dp0..\..\..\tests\docker-run-sample-test.bat" "user/samples/hello_world_cpp/test-hello-world-cpp.sh %~1" "%~2"
exit /b %errorlevel%
