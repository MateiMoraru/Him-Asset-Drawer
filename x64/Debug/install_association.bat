@echo off
echo Installing .him file association for HIM-Asset-Drawer...
echo.

:: Get the current directory path
set "EXEPATH=%~dp0HIM-Asset-Drawer.exe"

:: Check if executable exists
if not exist "%EXEPATH%" (
    echo ERROR: Could not find HIM-Asset-Drawer.exe in current directory
    echo Current directory: %~dp0
    pause
    exit /b 1
)

:: Register the file association (requires administrator privileges)
echo Registering .him extension...
reg add "HKEY_CLASSES_ROOT\.him" /ve /d "HIMAssetDrawerFile" /f >nul 2>&1

echo Creating file type...
reg add "HKEY_CLASSES_ROOT\HIMAssetDrawerFile" /ve /d "HIM Asset Drawer Image" /f >nul 2>&1

echo Setting open command...
reg add "HKEY_CLASSES_ROOT\HIMAssetDrawerFile\shell\open\command" /ve /d "\"%EXEPATH%\" \"%%1\"" /f >nul 2>&1

echo.
echo Association installed successfully!
echo.
echo You can now double-click .him files to open them with HIM-Asset-Drawer
echo.
echo Test it by double-clicking a .him file or run:
echo "%EXEPATH%" "C:\path\to\your\file.him"
echo.
pause