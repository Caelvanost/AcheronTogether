@echo off
setlocal EnableExtensions EnableDelayedExpansion
pushd "%~dp0" >nul
set "ROOT=%CD%"
set /p VERSION=<"%ROOT%\VERSION"
set "BUILD=%ROOT%\build"
set "DIST=%ROOT%\dist"
set "PACKAGE=%BUILD%\package"
set "ARCHIVE=%DIST%\AcheronTogether-v%VERSION%.zip"
set "SPRIGGIT_VERSION=0.40.1"

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

if not defined SKYRIM_ROOT (
    if exist "C:\Games\Steam\steamapps\common\Skyrim Special Edition\Papyrus Compiler\PapyrusCompiler.exe" set "SKYRIM_ROOT=C:\Games\Steam\steamapps\common\Skyrim Special Edition"
)
if not defined SKYRIM_ROOT (
    if exist "C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Papyrus Compiler\PapyrusCompiler.exe" set "SKYRIM_ROOT=C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition"
)

if not defined SKYRIM_ROOT (
    echo ERROR: Skyrim Special Edition was not found.
    echo Set SKYRIM_ROOT to the Skyrim Special Edition directory and retry.
    goto :fail
)

set "VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
set "PAPYRUS_COMPILER=%SKYRIM_ROOT%\Papyrus Compiler\PapyrusCompiler.exe"
set "PAPYRUS_SOURCE=%SKYRIM_ROOT%\Data\Source\Scripts"
set "PAPYRUS_FLAGS=%PAPYRUS_SOURCE%\TESV_Papyrus_Flags.flg"
set "SPRIGGIT_DIR=%BUILD%\tools\SpriggitCLI-%SPRIGGIT_VERSION%"
set "SPRIGGIT_EXE=%SPRIGGIT_DIR%\Spriggit.CLI.exe"
set "SPRIGGIT_ZIP=%BUILD%\tools\SpriggitCLI-%SPRIGGIT_VERSION%.zip"

echo Using vcpkg: %VCPKG_ROOT%
echo Using Skyrim: %SKYRIM_ROOT%

if not exist "%PAPYRUS_COMPILER%" (
    echo ERROR: PapyrusCompiler.exe not found at "%PAPYRUS_COMPILER%".
    goto :fail
)
if not exist "%PAPYRUS_FLAGS%" (
    echo ERROR: TESV_Papyrus_Flags.flg not found at "%PAPYRUS_FLAGS%".
    goto :fail
)
if not exist "%PAPYRUS_SOURCE%\SKI_ConfigBase.psc" (
    echo ERROR: SkyUI source scripts are required to compile the MCM.
    echo Expected: "%PAPYRUS_SOURCE%\SKI_ConfigBase.psc"
    goto :fail
)

if not exist "%DIST%" mkdir "%DIST%"
if exist "%PACKAGE%" rmdir /s /q "%PACKAGE%"
if exist "%ARCHIVE%" del /q "%ARCHIVE%"


echo.
echo [1/6] Configuring C++...
cmake -S "%ROOT%" -B "%BUILD%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%"
if errorlevel 1 goto :fail


echo.
echo [2/6] Building DLL...
cmake --build "%BUILD%" --config Release
if errorlevel 1 goto :fail

if not exist "%PACKAGE%\SKSE\Plugins\AcheronTogether.dll" (
    echo ERROR: packaged DLL was not produced.
    goto :fail
)


echo.
echo [3/6] Compiling Papyrus MCM...
if not exist "%PACKAGE%\Scripts" mkdir "%PACKAGE%\Scripts"
pushd "%ROOT%\Source\Scripts" >nul
"%PAPYRUS_COMPILER%" "AcheronTogetherNative.psc" -f="%PAPYRUS_FLAGS%" -i="%ROOT%\Source\Scripts;%PAPYRUS_SOURCE%" -o="%PACKAGE%\Scripts"
if errorlevel 1 (
    popd >nul
    goto :fail
)
"%PAPYRUS_COMPILER%" "AcheronTogetherMCM.psc" -f="%PAPYRUS_FLAGS%" -i="%ROOT%\Source\Scripts;%PAPYRUS_SOURCE%" -o="%PACKAGE%\Scripts"
if errorlevel 1 (
    popd >nul
    goto :fail
)
popd >nul

if not exist "%PACKAGE%\Scripts\AcheronTogetherNative.pex" (
    echo ERROR: AcheronTogetherNative.pex was not produced.
    goto :fail
)
if not exist "%PACKAGE%\Scripts\AcheronTogetherMCM.pex" (
    echo ERROR: AcheronTogetherMCM.pex was not produced.
    goto :fail
)


echo.
echo [4/6] Building lightweight MCM ESP...
if not exist "%SPRIGGIT_EXE%" (
    echo Spriggit %SPRIGGIT_VERSION% not found locally; downloading pinned CLI...
    if not exist "%BUILD%\tools" mkdir "%BUILD%\tools"
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Invoke-WebRequest -UseBasicParsing -Uri 'https://github.com/Mutagen-Modding/Spriggit/releases/download/%SPRIGGIT_VERSION%/SpriggitCLI.zip' -OutFile '%SPRIGGIT_ZIP%'"
    if errorlevel 1 goto :fail
    if exist "%SPRIGGIT_DIR%" rmdir /s /q "%SPRIGGIT_DIR%"
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -Path '%SPRIGGIT_ZIP%' -DestinationPath '%SPRIGGIT_DIR%' -Force"
    if errorlevel 1 goto :fail
)

if not exist "%SPRIGGIT_EXE%" (
    echo ERROR: Spriggit.CLI.exe was not found after extraction.
    goto :fail
)

"%SPRIGGIT_EXE%" deserialize --InputPath "%ROOT%\esp" --OutputPath "%PACKAGE%\AcheronTogether.esp"
if errorlevel 1 goto :fail
if not exist "%PACKAGE%\AcheronTogether.esp" (
    echo ERROR: AcheronTogether.esp was not produced.
    goto :fail
)


echo.
echo [5/6] Staging configuration...
copy /Y "%ROOT%\config\AcheronTogether.ini" "%PACKAGE%\SKSE\Plugins\AcheronTogether.ini" >nul
if errorlevel 1 goto :fail


echo.
echo [6/6] Creating Vortex archive...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%PACKAGE%\*' -DestinationPath '%ARCHIVE%' -Force"
if errorlevel 1 goto :fail


echo.
echo ============================================================
echo BUILD OK
 echo Archive: %ARCHIVE%
echo ============================================================
popd >nul
exit /b 0

:fail
echo.
echo BUILD FAILED.
popd >nul
exit /b 1
