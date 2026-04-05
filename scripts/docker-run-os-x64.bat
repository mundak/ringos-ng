@echo off
REM Build and run the ringos x64 OS in QEMU inside the shared Docker image.
REM Usage: scripts\docker-run-os-x64.bat

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
echo === Launching x64 OS in QEMU (%IMAGE_NAME%) ===
echo Press Ctrl+C to stop QEMU.
docker run --rm -it %TOOLCHAIN_TOKEN_ARG% -v "%HOST_BUILD_DIR%:/workspace/build" %IMAGE_NAME% bash -lc "bash tools/toolchain/download-latest-toolchain.sh --repo mundak/ringos-ng && cmake --preset x64-debug && cmake --build --preset build-x64-debug && scripts/run-x64.sh build/x64-debug/arch/x64/ringos_x64"
if %errorlevel% neq 0 (
    echo ERROR: Container exited with an error.
    exit /b %errorlevel%
)

echo.
echo === Done ===
