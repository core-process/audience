@echo off
cd %~dp0 || goto :error

rem read build type
set CMAKE_BUILD_TYPE=%1
for %%G in (Debug,Release,RelWithDebInfo,MinSizeRel) do (
  if "%CMAKE_BUILD_TYPE%" == "%%G" goto :build
)
echo Usage: %~0 ^<Debug^|Release^|RelWithDebInfo^|MinSizeRel^>
exit 1

:build

rem create build directory
if not exist ".\build" mkdir ".\build" || goto :error
cd ".\build" || goto :error

rem initialize cmake cache
if not exist .\CMakeCache.txt cmake .. || goto :error

rem perform build
cmake --build . --config "%CMAKE_BUILD_TYPE%" || goto :error

rem error and success handling
goto :EOF
:error
exit /b %errorlevel%
