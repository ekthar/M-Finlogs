@echo off
REM Build the installer for M-Finlogs Native
REM Prerequisites: 
REM   - Build completed: cmake --build native\build --config Release --target mfinlogs-native
REM   - Qt deployed: windeployqt run on the package folder
REM   - Inno Setup installed (iscc.exe in PATH or at default location)

set PROJECT_ROOT=%~dp0..\..
set BUILD_DIR=%PROJECT_ROOT%\native\build
set PACKAGE_DIR=%BUILD_DIR%\package
set INSTALLER_DIR=%PROJECT_ROOT%\native\installer

echo === Step 1: Create package directory ===
if exist "%PACKAGE_DIR%" rmdir /s /q "%PACKAGE_DIR%"
mkdir "%PACKAGE_DIR%"
mkdir "%PACKAGE_DIR%\fonts"

echo === Step 2: Copy executable and config ===
copy "%BUILD_DIR%\Release\mfinlogs-native.exe" "%PACKAGE_DIR%\" /Y
copy "%PROJECT_ROOT%\db_config.json" "%PACKAGE_DIR%\" /Y

echo === Step 3: Copy fonts ===
copy "%PROJECT_ROOT%\fonts\SpaceMono-Regular.ttf" "%PACKAGE_DIR%\fonts\" /Y
copy "%PROJECT_ROOT%\fonts\SpaceMono-Bold.ttf" "%PACKAGE_DIR%\fonts\" /Y
copy "%PROJECT_ROOT%\fonts\InterTight-Regular.ttf" "%PACKAGE_DIR%\fonts\" /Y
copy "%PROJECT_ROOT%\fonts\InterTight-Bold.ttf" "%PACKAGE_DIR%\fonts\" /Y

echo === Step 4: Run windeployqt ===
windeployqt --release --compiler-runtime "%PACKAGE_DIR%\mfinlogs-native.exe"

echo === Step 5: Build installer with Inno Setup ===
if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" "%INSTALLER_DIR%\setup.iss"
) else if exist "C:\Program Files\Inno Setup 6\ISCC.exe" (
    "C:\Program Files\Inno Setup 6\ISCC.exe" "%INSTALLER_DIR%\setup.iss"
) else (
    echo ERROR: Inno Setup not found. Install from https://jrsoftware.org/isdl.php
    echo Package is ready at: %PACKAGE_DIR%
    exit /b 1
)

echo === Done! Installer created in %INSTALLER_DIR%\Output\ ===
pause
