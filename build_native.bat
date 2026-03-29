@echo off
chcp 65001 >nul
echo ========================================
echo Noctis Viewer - Native Build
echo Ultra Lightweight (< 100 KB)
echo ========================================
echo.

REM Check for Visual Studio
where cl >nul 2>nul
if errorlevel 1 (
    echo [Error] Visual Studio compiler not found!
    echo.
    echo Please install Visual Studio with C++ workload:
    echo https://visualstudio.microsoft.com/downloads/
    echo.
    echo Or use MinGW-w64:
    echo https://www.mingw-w64.org/downloads/
    echo.
    pause
    exit /b 1
)

echo Compiler found: 
cl
echo.

REM Clean old builds
if exist "bin" rmdir /s /q "bin"
if exist "build" rmdir /s /q "build"

REM Create build directory
mkdir build
cd build

echo ========================================
echo Building with MSVC...
echo ========================================
echo.

REM Configure with CMake
cmake .. -G "Visual Studio 17 2022" -A x64

if errorlevel 1 (
    echo.
    echo [Error] CMake configuration failed!
    echo Trying direct compilation...
    echo.
    cd ..
    goto DIRECT_BUILD
)

REM Build
cmake --build . --config Release

if errorlevel 1 (
    echo.
    echo [Error] Build failed!
    cd ..
    exit /b 1
)

cd ..

REM Check result
if exist "bin\Noctis_Viewer.exe" (
    echo.
    echo ========================================
    echo ✅ Build SUCCESSFUL!
    echo ========================================
    echo.
    echo Output: bin\Noctis_Viewer.exe
    echo.
    echo File size:
    dir bin\Noctis_Viewer.exe | find "Noctis_Viewer.exe"
    echo.
    echo Features:
    echo - Pure Win32 API + GDI+
    echo - No external dependencies
    echo - Instant startup (< 0.1s)
    echo - Minimal memory usage
    echo.
) else (
    echo.
    echo ========================================
    echo ❌ Build FAILED!
    echo ========================================
    echo.
)

goto END

:DIRECT_BUILD
REM Direct compilation without CMake
echo Compiling directly with MSVC...
echo.

cl /EHsc /O2 /W4 ^
    /Fe:bin\Noctis_Viewer.exe ^
    /Fo:build\ ^
    noctis_viewer.cpp ^
    gdiplus.lib user32.lib shell32.lib ole32.lib comctl32.lib

if errorlevel 1 (
    echo.
    echo [Error] Direct compilation failed!
    exit /b 1
)

:END
pause
