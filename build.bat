@echo off
SETLOCAL ENABLEEXTENSIONS

where ninja.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: ninja.exe was not found on PATH.
    pause
    exit /b 1
)

echo Using Ninja:
where ninja.exe
echo.

if not exist "build\CMakeCache.txt" (
    echo Configuring Release build...
    cmake --preset windows
    if errorlevel 1 goto :fail
)

echo Building Release...
cmake --build --preset windows
if errorlevel 1 goto :fail

echo.
echo Build successful!
echo Output: build\mtdetect.sys build\mttest.sys
goto :end

:fail
echo.
echo Build failed.
pause
exit /b 1

:end
ENDLOCAL
