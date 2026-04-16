@echo off
REM Build the shared ringos SDK package for all targets as a distributable tar.xz archive on Windows.
REM Usage: user\sdk\docker-build-sdk.bat

setlocal EnableExtensions EnableDelayedExpansion

set IMAGE_NAME=ringos-ci-sdk-release
pushd "%~dp0..\.."
set CONTEXT_DIR=%cd%
popd
set OUTPUT_DIR=%CONTEXT_DIR%\build
set OUTPUT_ARCHIVE=

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

if not exist "%OUTPUT_DIR%" (
    mkdir "%OUTPUT_DIR%"
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
echo === Building shared SDK archive under %OUTPUT_DIR% ===
docker run --rm !DOCKER_ENV_ARGS! -v "%OUTPUT_DIR%:/workspace/build" %IMAGE_NAME% bash -lc "set -euo pipefail; user/sdk/build-sdk.sh !RELEASE_ARGS! --output-dir /workspace/build"
if %errorlevel% neq 0 (
    echo ERROR: Container exited with an error.
    exit /b %errorlevel%
)

for /f "delims=" %%I in ('dir /b /a-d /o-d "%OUTPUT_DIR%\ringos-sdk-*.tar.xz"') do (
    if not defined OUTPUT_ARCHIVE (
        set OUTPUT_ARCHIVE=%OUTPUT_DIR%\%%I
    )
)

if not defined OUTPUT_ARCHIVE (
    echo ERROR: Unable to locate the generated versioned SDK archive under %OUTPUT_DIR%.
    exit /b 1
)

echo.
echo Shared SDK archive: %OUTPUT_ARCHIVE%
echo Host build directory: %OUTPUT_DIR%
echo === Done ===
