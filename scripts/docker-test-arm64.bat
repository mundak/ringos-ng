@echo off
REM Build and test the ringos arm64 target inside a Docker container.
REM Usage: scripts\docker-test-arm64.bat

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
echo === Running arm64 tests in %IMAGE_NAME% container ===
docker run --rm %TOOLCHAIN_TOKEN_ARG% -v "%HOST_RINGOS_CACHE%:/root/.cache/ringos" %IMAGE_NAME% bash -lc "bash tools/toolchain/ensure-toolchain-release.sh --repo mundak/ringos-ng --allow-build && cmake --preset arm64-debug && cmake --build --preset build-arm64-debug && ctest --preset smoke_arm64_native && ctest --preset smoke_arm64_ansi_c && ctest --preset smoke_arm64_x64_emulator"
if %errorlevel% neq 0 (
    echo ERROR: Container exited with an error.
    exit /b %errorlevel%
)

echo.
echo === Done ===
