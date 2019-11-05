@echo off
cd %~dp0 || goto :error

rem build MinSizeRel
call .\build.bat MinSizeRel

rem create cli runtime
if exist .\dist\runtime-cli rmdir /S /Q .\dist\runtime-cli || goto :error
xcopy /S .\dist\MinSizeRel\bin .\dist\runtime-cli || goto :error
del .\dist\runtime-cli\audience_shared.dll || goto :error

rem create dynamic runtime
if exist .\dist\runtime-dynamic rmdir /S /Q .\dist\runtime-dynamic || goto :error
xcopy /S .\dist\MinSizeRel\bin .\dist\runtime-dynamic || goto :error
del .\dist\runtime-dynamic\audience.exe || goto :error

rem create static runtime
if exist .\dist\runtime-static rmdir /S /Q .\dist\runtime-static || goto :error
xcopy /S .\dist\MinSizeRel\bin .\dist\runtime-static || goto :error
del .\dist\runtime-static\audience.exe || goto :error
del .\dist\runtime-static\audience_shared.dll || goto :error

rem error and success handling
goto :EOF
:error
exit /b %errorlevel%
