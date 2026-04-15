@echo off
REM Build the shared ringos SDK package for all targets as a distributable tar.xz archive on Windows.
REM Usage: user\sdk\docker-build-sdk.bat

setlocal EnableExtensions EnableDelayedExpansion

set IMAGE_NAME=ringos-ci-sdk-release
set CONTEXT_DIR=%~dp0..\..
set BUILD_VOLUME_NAME=%RINGOS_SDK_BUILD_VOLUME%
set OUTPUT_DIR=%CONTEXT_DIR%\build
set OUTPUT_ARCHIVE=

if not defined BUILD_VOLUME_NAME (
    set BUILD_VOLUME_NAME=ringos-sdk-build
)

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

docker volume inspect "%BUILD_VOLUME_NAME%" >nul 2>&1
if %errorlevel% neq 0 (
    echo === Creating Docker build volume: %BUILD_VOLUME_NAME% ===
    docker volume create "%BUILD_VOLUME_NAME%" >nul
    if errorlevel 1 (
        echo ERROR: Docker volume creation failed.
        exit /b %errorlevel%
    )
)

echo.
echo === Building shared SDK archive under %OUTPUT_DIR% ===
docker run --rm !DOCKER_ENV_ARGS! -v "%BUILD_VOLUME_NAME%:/workspace/build" -v "%OUTPUT_DIR%:/sdk-output" -v "%OUTPUT_DIR%:/host-build" %IMAGE_NAME% bash -lc "set -euo pipefail; if ls /host-build/ringos-toolchain-*.tar.xz ^>/dev/null 2^>^&1; then cp /host-build/ringos-toolchain-*.tar.xz /workspace/build/; fi; status=0; user/sdk/build-sdk.sh !RELEASE_ARGS! --output-dir /sdk-output || status=$?; if ls /workspace/build/ringos-toolchain-*.tar.xz ^>/dev/null 2^>^&1; then cp /workspace/build/ringos-toolchain-*.tar.xz /host-build/; fi; exit $status"
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
echo Persistent Docker build volume: %BUILD_VOLUME_NAME%
echo === Done ===
