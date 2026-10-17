@echo off
setlocal
cd /d "%~dp0"
set "RA2_NO_PAUSE=1"
call BUILD_ZIG.cmd
if errorlevel 1 exit /b 1
set "RA2_PYTHON=python"
if exist "..\runtime\python.exe" set "RA2_PYTHON=..\runtime\python.exe"
"%RA2_PYTHON%" tools\verify_windows_outputs.py
if errorlevel 1 exit /b 1
echo Current regression evidence is required before packaging a rebuilt DLL.
echo See docs and verification. Old DLL hashes will be rejected.
"%RA2_PYTHON%" tools\package_full_release.py
exit /b %errorlevel%
