@echo off
rem GasStoveUE5 installer — pulls latest pack from git, installs into a UE project.
rem Usage: double-click (will ask for .uproject) or drag-drop .uproject onto this file.
setlocal
set ROOT=%~dp0
if "%ROOT:~-1%"=="\" set ROOT=%ROOT:~0,-1%

echo [1/2] Updating pack from git...
where git >nul 2>nul
if errorlevel 1 (
  echo   git not found, skipping self-update.
) else (
  pushd "%ROOT%"
  git pull --ff-only
  popd
)

echo [2/2] Installing...
if not "%~1"=="" (
  set TARGET=%~1
) else (
  set /p TARGET=Path to target .uproject:
)
where python >nul 2>nul
if errorlevel 1 (
  echo Python not found. Install Python 3 from https://www.python.org/downloads/
  pause
  exit /b 1
)
python "%ROOT%\Python\install_gas_burner.py" --source "%ROOT%" --target "%TARGET%"
pause
