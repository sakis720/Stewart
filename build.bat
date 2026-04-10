@echo off
setlocal
echo === Stewart Build Script ===

if not exist build (
    mkdir build
)

cd build
echo Configuring with CMake...
cmake ..
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b %ERRORLEVEL%
)

echo Building Project (Release)...
cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed.
    pause
    exit /b %ERRORLEVEL%
)

cd ..
echo.
echo === Build Successful ===
echo Executable located at: build\Release\Stewart.exe
echo.
pause
