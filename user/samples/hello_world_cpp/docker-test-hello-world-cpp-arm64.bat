@echo off
call "%~dp0..\..\..\tests\docker-run-sample-test.bat" "user/samples/hello_world_cpp/test-hello-world-cpp-arm64.sh"
exit /b %errorlevel%
