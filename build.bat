@echo off
setlocal enabledelayedexpansion

set "TARGET_EXE=windows_ui_demo.exe"
set "BUILD_TYPE="
set "CLEAN_REQUESTED="
set "EIGENMATH_FLAG="

:ParseArgs
if "%~1"=="" goto :DoneParsing
if "%~1"=="clean" ( set "CLEAN_REQUESTED=1" & shift & goto :ParseArgs )
if "%~1"=="debug" ( set "BUILD_TYPE=Debug" & shift & goto :ParseArgs )
if "%~1"=="release" ( set "BUILD_TYPE=Release" & shift & goto :ParseArgs )
if "%~1"=="eigenmath" ( set "EIGENMATH_FLAG=-DMATHCORE_ENABLE_EIGENMATH=ON" & shift & goto :ParseArgs )
echo Unknown argument: %1
exit /b 1
:DoneParsing

if not defined BUILD_TYPE (
    echo   [1] Release  - optimized ^(default^)
    echo   [2] Debug    - unoptimized, full debug symbols
    set "BUILD_CHOICE="
    set /p "BUILD_CHOICE=Enter 1 or 2 (Enter = Release): "
    if "!BUILD_CHOICE!"=="2" ( set "BUILD_TYPE=Debug" ) else ( set "BUILD_TYPE=Release" )
)
echo [Config] Build type: %BUILD_TYPE%

if /i "%BUILD_TYPE%"=="Debug" ( set "BUILD_DIR=build_debug" ) else ( set "BUILD_DIR=build" )

if defined CLEAN_REQUESTED (
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)
if not defined VS_PATH (
    echo ERROR: Visual Studio Build Tools not found.
    exit /b 1
)
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo ERROR: Failed to initialize MSVC environment. & exit /b 1 )

where git >nul 2>&1
if not errorlevel 1 (
    git submodule update --init --recursive
    if errorlevel 1 ( echo ERROR: submodule update failed. & exit /b 1 )
)

cmake -G Ninja -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% %EIGENMATH_FLAG%
if errorlevel 1 ( echo ERROR: CMake configuration failed. & exit /b 1 )

cmake --build "%BUILD_DIR%" --parallel
if errorlevel 1 ( echo ERROR: Build failed. & exit /b 1 )

echo Success! Output: %BUILD_DIR%\%TARGET_EXE%
endlocal
exit /b 0
