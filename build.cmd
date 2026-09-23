@echo off
setlocal

set BUILD_DIR=build\Release

echo ============================================
echo  Step 1/2: CMake Configure (Ninja) ...
echo ============================================
cmake -G Ninja -S . -B %BUILD_DIR% -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo [ERROR] CMake configure failed !
    exit /b %errorlevel%
)

echo.
echo ============================================
echo  Step 2/2: Ninja Build ...
echo ============================================
cmake --build %BUILD_DIR%
if %errorlevel% neq 0 (
    echo [ERROR] Build failed !
    exit /b %errorlevel%
)

echo.
echo ============================================
echo  Build SUCCESS !
echo  Output: %BUILD_DIR%\application.bin
echo ============================================
endlocal