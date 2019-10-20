@echo off
cd %~dp0 || goto :error

rem remove all build files
if exist .\build rmdir /S /Q .\build || goto :error

rem error and success handling
goto :EOF
:error
exit /b %errorlevel%
