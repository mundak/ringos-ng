@echo off
REM Build and test the ringos x64 target inside a Docker container.
REM Usage: scripts\docker-test-x64.bat

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
echo === Running x64 tests in %IMAGE_NAME% container ===
docker run --rm %IMAGE_NAME% bash -lc "cmake --preset x64-debug && cmake --build --preset build-x64-debug && ctest --preset x64_emulator_unit && ctest --preset x64_win32_loader_unit && ctest --preset smoke_x64_native"
if %errorlevel% neq 0 (
    echo ERROR: Container exited with an error.
    exit /b %errorlevel%
)

echo.
echo === Done ===
