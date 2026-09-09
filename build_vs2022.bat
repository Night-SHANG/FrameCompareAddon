@echo off
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
  echo [FrameCompare] CMake was not found in PATH.
  exit /b 1
)

cmake -S . -B build -A x64
if errorlevel 1 exit /b %errorlevel%

cmake --build build --config Release --parallel
if errorlevel 1 exit /b %errorlevel%

echo.
echo [FrameCompare] Build complete.
echo Package: build\Release\package\
endlocal
