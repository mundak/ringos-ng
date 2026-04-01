@echo off
REM Build and run the console_service_write arm64 native sample test inside a Docker container.
REM Usage: tests\docker-test-console-service-write-arm64-native.bat

setlocal

set IMAGE_NAME=ringos-ci
set CONTEXT_DIR=%~dp0..

if defined LOCALAPPDATA (
    set HOST_RINGOS_CACHE=%LOCALAPPDATA%\ringos
) else (
    set HOST_RINGOS_CACHE=%CONTEXT_DIR%\build\ringos-cache
)

if not exist "%HOST_RINGOS_CACHE%" (
    mkdir "%HOST_RINGOS_CACHE%"
)

set TOOLCHAIN_TOKEN_ARG=
if defined GH_TOKEN (
    set TOOLCHAIN_TOKEN_ARG=-e GH_TOKEN
) else if defined GITHUB_TOKEN (
    set TOOLCHAIN_TOKEN_ARG=-e GITHUB_TOKEN
)

echo === Building Docker image: %IMAGE_NAME% ===
docker build -f "%CONTEXT_DIR%\docker\Dockerfile" -t %IMAGE_NAME% "%CONTEXT_DIR%"
if %errorlevel% neq 0 (
    echo ERROR: Docker image build failed.
    exit /b %errorlevel%
)

echo.
echo === Running console_service_write arm64 native sample test in %IMAGE_NAME% container ===
docker run --rm %TOOLCHAIN_TOKEN_ARG% -v "%HOST_RINGOS_CACHE%:/root/.cache/ringos" %IMAGE_NAME% bash -lc "bash tools/toolchain/ensure-toolchain-release.sh --repo mundak/ringos-ng --allow-build && cmake --preset arm64-debug && cmake --build --preset build-arm64-debug --target ringos_arm64_console_service_write && ctest --preset sample_console_service_write_arm64_native"
if %errorlevel% neq 0 (
    echo ERROR: Container exited with an error.
    exit /b %errorlevel%
)

echo.
echo === Done ===
