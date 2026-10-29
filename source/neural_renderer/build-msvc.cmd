@echo off
setlocal EnableExtensions EnableDelayedExpansion
if not defined RA2_NGX_SDK (echo Set RA2_NGX_SDK to the separately prepared official NGX development SDK. & exit /b 2)
if not defined RA2_NEURAL_OUTPUT set "RA2_NEURAL_OUTPUT=%~dp0RA2NeuralRenderer.exe"
for %%i in ("%RA2_NGX_SDK%") do set "RA2_NGX_SDK=%%~fi"
for %%i in ("%RA2_NEURAL_OUTPUT%") do set "RA2_NEURAL_OUTPUT=%%~fi"
if /i not "%VSCMD_ARG_TGT_ARCH%"=="x64" (
 for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "RA2_NEURAL_VS=%%i"
 if not defined RA2_NEURAL_VS (echo Visual Studio 2022 C++ tools are required. & exit /b 2)
 call "!RA2_NEURAL_VS!\VC\Auxiliary\Build\vcvarsall.bat" x64
 if errorlevel 1 exit /b 2
)
pushd "%~dp0source"
set "RA2_NEURAL_OBJ=%TEMP%\ra2-neural-build-%RANDOM%-%RANDOM%"
mkdir "%RA2_NEURAL_OBJ%" || exit /b 2
rc /nologo /fo "%RA2_NEURAL_OBJ%\app.res" app.rc
if errorlevel 1 goto failed
cl /nologo /std:c++17 /O2 /MT /EHsc /GR- /DUNICODE /D_UNICODE /I"%RA2_NGX_SDK%\include" main.cpp /Fo"%RA2_NEURAL_OBJ%\main.obj" /Fe"%RA2_NEURAL_OUTPUT%" /link "%RA2_NGX_SDK%\lib\Windows_x86_64\x64\nvsdk_ngx_s.lib" "%RA2_NEURAL_OBJ%\app.res" kernel32.lib user32.lib advapi32.lib shell32.lib ole32.lib shlwapi.lib version.lib wintrust.lib crypt32.lib uuid.lib /SUBSYSTEM:CONSOLE /MANIFEST:NO /INCREMENTAL:NO /OPT:REF /OPT:ICF
if errorlevel 1 goto failed
rmdir /s /q "%RA2_NEURAL_OBJ%"
popd
exit /b 0
:failed
set "RA2_NEURAL_ERROR=%ERRORLEVEL%"
if "%RA2_NEURAL_ERROR%"=="0" set "RA2_NEURAL_ERROR=1"
rmdir /s /q "%RA2_NEURAL_OBJ%"
popd
exit /b %RA2_NEURAL_ERROR%
