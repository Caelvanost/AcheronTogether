@echo off
setlocal enabledelayedexpansion
pushd "%~dp0" >nul
set "ROOT=%CD%"
set /p VERSION=<"%ROOT%\VERSION"
set "BUILD=%ROOT%\build"
set "DIST=%ROOT%\dist"
set "PACKAGE=%BUILD%\package"
set "ARCHIVE=%DIST%\AcheronTogether-v%VERSION%.zip"

echo ============================================================
echo  Acheron Together v%VERSION% - Release Build
echo ============================================================

if not defined VCPKG_ROOT (
    if exist "C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake" set "VCPKG_ROOT=C:\dev\vcpkg"
)
if not defined VCPKG_ROOT (
    if exist "C:\vcpkg\scripts\buildsystems\vcpkg.cmake" set "VCPKG_ROOT=C:\vcpkg"
)
if not defined VCPKG_ROOT (
    if exist "%USERPROFILE%\vcpkg\scripts\buildsystems\vcpkg.cmake" set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
)

if not defined VCPKG_ROOT (
    echo ERROR: vcpkg was not found.
    echo Set VCPKG_ROOT and retry.
    goto :fail
)

set "VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
echo Using vcpkg: %VCPKG_ROOT%

if not exist "%DIST%" mkdir "%DIST%"
if exist "%PACKAGE%" rmdir /s /q "%PACKAGE%"
if exist "%ARCHIVE%" del /q "%ARCHIVE%"

echo.
echo [1/4] Configuring...
cmake -S "%ROOT%" -B "%BUILD%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%"
if errorlevel 1 goto :fail

echo.
echo [2/4] Building...
cmake --build "%BUILD%" --config Release
if errorlevel 1 goto :fail

if not exist "%PACKAGE%\SKSE\Plugins\AcheronTogether.dll" (
    echo ERROR: packaged DLL was not produced.
    goto :fail
)

echo.
echo [3/4] Creating Vortex archive...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%PACKAGE%\*' -DestinationPath '%ARCHIVE%' -Force"
if errorlevel 1 goto :fail

echo.
echo [4/4] Done.
echo Archive: %ARCHIVE%
popd >nul
exit /b 0

:fail
echo.
echo BUILD FAILED.
popd >nul
exit /b 1
