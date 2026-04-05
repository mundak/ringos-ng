@echo off
REM Build and run the hello_world x64 native sample test inside a Docker container.
REM Usage: tests\docker-test-hello-world-x64-native.bat

setlocal

set IMAGE_NAME=ringos-ci
set CONTEXT_DIR=%~dp0..
set HOST_BUILD_DIR=%CONTEXT_DIR%\build

if not exist "%HOST_BUILD_DIR%" (
    mkdir "%HOST_BUILD_DIR%"
)

set TOOLCHAIN_TOKEN_ARG=
if defined GH_TOKEN (
    set TOOLCHAIN_TOKEN_ARG=-e GH_TOKEN
) else if defined GITHUB_TOKEN (
    set TOOLCHAIN_TOKEN_ARG=-e GITHUB_TOKEN
)

echo === Building Docker image: %IMAGE_NAME% ===
docker build -f "%CONTEXT_DIR%\tools\toolchain\Dockerfile" -t %IMAGE_NAME% "%CONTEXT_DIR%"
if %errorlevel% neq 0 (
    echo ERROR: Docker image build failed.
    exit /b %errorlevel%
)

echo.
echo === Running hello_world x64 native sample test in %IMAGE_NAME% container ===
docker run --rm %TOOLCHAIN_TOKEN_ARG% -v "%HOST_BUILD_DIR%:/workspace/build" %IMAGE_NAME% bash -lc "bash tools/toolchain/download-latest-toolchain.sh --repo mundak/ringos-ng && cmake --preset x64-debug && cmake --build --preset build-x64-debug --target ringos_x64 && ctest --preset sample_hello_world_x64_native"
if %errorlevel% neq 0 (
    echo ERROR: Container exited with an error.
    exit /b %errorlevel%
)

echo.
echo === Done ===
