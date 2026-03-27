@echo off
REM Build and test the ringos arm64 target inside a Docker container.
REM Usage: scripts\docker-test-arm64.bat

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
echo === Running arm64 tests in %IMAGE_NAME% container ===
docker run --rm %IMAGE_NAME% bash -lc "cmake --preset arm64-ci && cmake --build --preset build-arm64-ci && ctest --preset test-arm64-ci"
if %errorlevel% neq 0 (
    echo ERROR: Container exited with an error.
    exit /b %errorlevel%
)

echo.
echo === Done ===
