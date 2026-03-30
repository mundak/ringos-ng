@echo off
REM Build the shared ringos toolchain package for all targets as a distributable ZIP on Windows.
REM Usage: tools\toolchain\docker-build-toolchain.bat [output-zip]

setlocal

set IMAGE_NAME=ringos-ci
set CONTEXT_DIR=%~dp0..\..

if "%~1"=="" (
    set OUTPUT_ZIP=%CONTEXT_DIR%\build\ringos-toolchain.zip
) else (
    set OUTPUT_ZIP=%~1
)

for %%I in ("%OUTPUT_ZIP%") do (
    set OUTPUT_DIR=%%~dpI
    set OUTPUT_NAME=%%~nxI
)

if not exist "%OUTPUT_DIR%" (
    mkdir "%OUTPUT_DIR%"
)

echo === Building Docker image: %IMAGE_NAME% ===
docker build -f "%CONTEXT_DIR%\docker\Dockerfile" -t %IMAGE_NAME% "%CONTEXT_DIR%"
if %errorlevel% neq 0 (
    echo ERROR: Docker image build failed.
    exit /b %errorlevel%
)

echo.
echo === Building shared toolchain archive at %OUTPUT_ZIP% ===
docker run --rm -v "%OUTPUT_DIR%:/toolchain-output" %IMAGE_NAME% bash -lc "tools/toolchain/build-toolchain.sh /toolchain-output/%OUTPUT_NAME%"
if %errorlevel% neq 0 (
    echo ERROR: Container exited with an error.
    exit /b %errorlevel%
)

echo.
echo Shared toolchain archive: %OUTPUT_ZIP%
echo === Done ===
