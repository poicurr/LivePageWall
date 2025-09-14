@echo off
setlocal

set "VCVARS_BAT="
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
  for /f "usebackq tokens=* delims=" %%I in (`
    "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * ^
      -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  `) do (
    if exist "%%I\VC\Auxiliary\Build\vcvars64.bat" (
      set "VCVARS_BAT=%%I\VC\Auxiliary\Build\vcvars64.bat"
    )
  )
)

if not defined VCVARS_BAT if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
  set "VCVARS_BAT=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)

if not defined VCVARS_BAT (
  echo [vcpp] vcvars64.bat not found
  exit /b 1
)

call "%VCVARS_BAT%" >nul 2>&1
if errorlevel 1 (
  echo [vcpp] vcvars64.bat failed
  exit /b 1
)

cl %*
set "rc=%ERRORLEVEL%"

endlocal & exit /b %rc%
