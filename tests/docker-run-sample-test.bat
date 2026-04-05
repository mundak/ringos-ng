@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%~1"=="" (
    echo Usage: tests\docker-run-sample-test.bat ^<linux-test-script^> [docker-image-name]
    exit /b 1
)

set TEST_SCRIPT=%~1
if "%~2"=="" (
    set IMAGE_NAME=ringos-sample-tests
) else (
    set IMAGE_NAME=%~2
)

for %%I in ("%~dp0..") do (
    set CONTEXT_DIR=%%~fI
)

set BUILD_DIR=%CONTEXT_DIR%\build
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
)

set DOCKER_ENV_ARGS=
if defined GH_TOKEN (
    set DOCKER_ENV_ARGS=!DOCKER_ENV_ARGS! -e GH_TOKEN
)
if defined GITHUB_TOKEN (
    set DOCKER_ENV_ARGS=!DOCKER_ENV_ARGS! -e GITHUB_TOKEN
)
if defined GITHUB_REPOSITORY (
    set DOCKER_ENV_ARGS=!DOCKER_ENV_ARGS! -e GITHUB_REPOSITORY
)

echo === Building Docker image: %IMAGE_NAME% ===
docker build -f "%CONTEXT_DIR%\tests\tests.Dockerfile" -t %IMAGE_NAME% "%CONTEXT_DIR%"
if errorlevel 1 (
    echo ERROR: Docker image build failed.
    exit /b %errorlevel%
)

echo === Running %TEST_SCRIPT% ===
docker run --rm !DOCKER_ENV_ARGS! -v "%BUILD_DIR%:/workspace/build" %IMAGE_NAME% bash -lc "%TEST_SCRIPT%"
if errorlevel 1 (
    echo ERROR: Docker test failed.
    exit /b %errorlevel%
)

echo === Done ===
