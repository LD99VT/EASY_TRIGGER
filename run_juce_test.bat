@echo off
setlocal
cd /d "%~dp0"

echo [1/3] Build started...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_win.ps1"
if errorlevel 1 (
  echo.
  echo Build failed. Press any key to close.
  pause >nul
  exit /b 1
)

set "EXE=%~dp0build\windows-msvc\MTC_Bridge_artefacts\Release\Easy Bridge v2.exe"
if not exist "%EXE%" (
  echo.
  echo EXE not found:
  echo %EXE%
  echo Press any key to close.
  pause >nul
  exit /b 1
)

echo [2/3] Build OK.
echo [3/3] Starting app...
start "" "%EXE%"
echo Done.
endlocal
