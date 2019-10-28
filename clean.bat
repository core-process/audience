@echo off
cd %~dp0 || goto :error

rem remove all build and dist files
if exist .\build rmdir /S /Q .\build || goto :error
if exist .\dist rmdir /S /Q .\dist || goto :error

rem error and success handling
goto :EOF
:error
exit /b %errorlevel%
