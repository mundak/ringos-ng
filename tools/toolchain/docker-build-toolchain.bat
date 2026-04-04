@echo off
REM Build the shared ringos toolchain package for all targets as a distributable tar.xz archive on Windows.
REM Usage: tools\toolchain\docker-build-toolchain.bat [output-archive]

setlocal EnableExtensions EnableDelayedExpansion

set IMAGE_NAME=ringos-ci
set CONTEXT_DIR=%~dp0..\..

if "%~1"=="" (
    set OUTPUT_DIR=%CONTEXT_DIR%\build
    set OUTPUT_ARCHIVE=
) else (
    set OUTPUT_ARCHIVE=%~1
    for %%I in ("%OUTPUT_ARCHIVE%") do (
        set OUTPUT_DIR=%%~dpI
        set OUTPUT_NAME=%%~nxI
    )
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
if "%~1"=="" (
    echo === Building shared toolchain archive under %OUTPUT_DIR% ===
    docker run --rm -v "%OUTPUT_DIR%:/workspace/build" %IMAGE_NAME% bash -lc "tools/toolchain/build-toolchain.sh"
) else (
    echo === Building shared toolchain archive at %OUTPUT_ARCHIVE% ===
    docker run --rm -v "%OUTPUT_DIR%:/toolchain-output" %IMAGE_NAME% bash -lc "tools/toolchain/build-toolchain.sh /toolchain-output/%OUTPUT_NAME%"
)
if %errorlevel% neq 0 (
    echo ERROR: Container exited with an error.
    exit /b %errorlevel%
)

if "%~1"=="" (
    for /f "delims=" %%I in ('dir /b /a-d /o-d "%OUTPUT_DIR%\ringos-toolchain-*.tar.xz"') do (
        if not defined OUTPUT_ARCHIVE (
            set OUTPUT_ARCHIVE=%OUTPUT_DIR%\%%I
        )
    )
)

if not defined OUTPUT_ARCHIVE (
    echo ERROR: Unable to locate the generated versioned toolchain archive under %OUTPUT_DIR%.
    exit /b 1
)

echo.
echo Shared toolchain archive: %OUTPUT_ARCHIVE%
echo === Done ===
