@echo off
REM Build and run the ringos x64 OS in QEMU inside the shared Docker image.
REM Usage: scripts\docker-run-os-x64.bat

setlocal

set IMAGE_NAME=ringos-ci
set CONTEXT_DIR=%~dp0..

echo === Building Docker image: %IMAGE_NAME% ===
docker build -f "%CONTEXT_DIR%\docker\Dockerfile" -t %IMAGE_NAME% "%CONTEXT_DIR%"
if %errorlevel% neq 0 (
    echo ERROR: Docker image build failed.
    exit /b %errorlevel%
)

echo.
echo === Launching x64 OS in QEMU (%IMAGE_NAME%) ===
echo Press Ctrl+C to stop QEMU.
docker run --rm -it %IMAGE_NAME% bash -lc "cmake --preset x64-ci && cmake --build --preset build-x64-ci && scripts/run-x64.sh build/x64-ci/arch/x64/ringos_x64"
if %errorlevel% neq 0 (
    echo ERROR: Container exited with an error.
    exit /b %errorlevel%
)

echo.
echo === Done ===
