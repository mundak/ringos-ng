@echo off
REM Build the shared ringos toolchain package for all targets as a distributable tar.xz archive on Windows.
REM Usage: tools\toolchain\docker-build-toolchain.bat [output-archive]

setlocal EnableExtensions EnableDelayedExpansion

set IMAGE_NAME=ringos-ci-toolchain-release
set CONTEXT_DIR=%~dp0..\..
set HOST_BUILD_DIR=%CONTEXT_DIR%\build

set RELEASE_REPO=
if defined GITHUB_REPOSITORY (
    set RELEASE_REPO=%GITHUB_REPOSITORY%
) else (
    for /f "delims=" %%I in ('gh repo view --json nameWithOwner -q .nameWithOwner 2^>nul') do (
        if not defined RELEASE_REPO (
            set RELEASE_REPO=%%I
        )
    )
)

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

if not exist "%HOST_BUILD_DIR%" (
    mkdir "%HOST_BUILD_DIR%"
)

set DOCKER_ENV_ARGS=
if defined GH_TOKEN (
    set DOCKER_ENV_ARGS=!DOCKER_ENV_ARGS! -e GH_TOKEN
)
if defined GITHUB_TOKEN (
    set DOCKER_ENV_ARGS=!DOCKER_ENV_ARGS! -e GITHUB_TOKEN
)
if defined RELEASE_REPO (
    set DOCKER_ENV_ARGS=!DOCKER_ENV_ARGS! -e GITHUB_REPOSITORY=%RELEASE_REPO%
)

set RELEASE_ARGS=
if defined RELEASE_REPO (
    set RELEASE_ARGS=--repo %RELEASE_REPO%
)

echo === Building Docker image: %IMAGE_NAME% ===
docker build -f "%CONTEXT_DIR%\tools\toolchain\Dockerfile" -t %IMAGE_NAME% "%CONTEXT_DIR%"
if %errorlevel% neq 0 (
    echo ERROR: Docker image build failed.
    exit /b %errorlevel%
)

echo.
if "%~1"=="" (
    echo === Building shared toolchain archive under %OUTPUT_DIR% ===
    docker run --rm !DOCKER_ENV_ARGS! -v "%HOST_BUILD_DIR%:/workspace/build" %IMAGE_NAME% bash -lc "tools/toolchain/run-toolchain-release.sh !RELEASE_ARGS! --output-dir /workspace/build"
) else (
    echo === Building shared toolchain archive at %OUTPUT_ARCHIVE% ===
    docker run --rm !DOCKER_ENV_ARGS! -v "%HOST_BUILD_DIR%:/workspace/build" -v "%OUTPUT_DIR%:/toolchain-output" %IMAGE_NAME% bash -lc "tools/toolchain/run-toolchain-release.sh !RELEASE_ARGS! --output-archive /toolchain-output/%OUTPUT_NAME%"
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
