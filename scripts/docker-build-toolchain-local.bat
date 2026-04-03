@echo off
REM Build the native LLVM toolchain and installed RingOS bundle in Docker using
REM a persistent host-side cache so iterative LLVM patch work can reuse the
REM cloned source tree, Ninja build directory, and previous-stage install root.
REM Usage: scripts\docker-build-toolchain-local.bat

setlocal

set IMAGE_NAME=ringos-ci-native-llvm-local
set CONTEXT_DIR=%~dp0..

if defined LOCALAPPDATA (
    set HOST_RINGOS_CACHE=%LOCALAPPDATA%\ringos
) else (
    set HOST_RINGOS_CACHE=%CONTEXT_DIR%\build\ringos-cache
)

if not exist "%HOST_RINGOS_CACHE%" (
    mkdir "%HOST_RINGOS_CACHE%"
)

echo === Building Docker image: %IMAGE_NAME% ===
docker build -f "%CONTEXT_DIR%\docker\Dockerfile" -t %IMAGE_NAME% "%CONTEXT_DIR%"
if %errorlevel% neq 0 (
    echo ERROR: Docker image build failed.
    exit /b %errorlevel%
)

echo.
echo === Building cached local toolchain bundle in Docker (%IMAGE_NAME%) ===
docker run --rm -it ^
  -v "%CONTEXT_DIR%:/workspace" ^
  -v "%HOST_RINGOS_CACHE%:/root/.cache/ringos" ^
  %IMAGE_NAME% ^
  bash -lc "set -euo pipefail; cd /workspace; dos2unix tools/llvm/build-clang-toolchain.sh tools/toolchain/build-toolchain.sh tools/toolchain/build-toolchain-local.sh >/dev/null; bash tools/toolchain/build-toolchain-local.sh"
if %errorlevel% neq 0 (
    echo ERROR: Container exited with an error.
    exit /b %errorlevel%
)

echo.
echo === Done ===
