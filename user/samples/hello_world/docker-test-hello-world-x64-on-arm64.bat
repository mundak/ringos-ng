@echo off
call "%~dp0..\..\..\tests\docker-run-sample-test.bat" "user/samples/hello_world/test-hello-world-x64-on-arm64.sh"
exit /b %errorlevel%
