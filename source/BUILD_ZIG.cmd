@echo off
setlocal
cd /d "%~dp0"
rem Never retain a successful-build marker across a partial rebuild.
del /q "BUILD-VERIFIED-v120.tag" "verification\BUILD-VERIFIED-v120.tag" "WINDOWS_BUILD_ARTIFACTS.json" "verification\WINDOWS_BUILD_ARTIFACTS.json" 2>nul
if defined ZIG_EXE (set "ZIG=%ZIG_EXE%") else (set "ZIG=zig")
"%ZIG%" version
if errorlevel 1 goto missing
set "RA2_ZIG_VERSION="
"%ZIG%" version > "%~dp0zig-version.tmp"
if errorlevel 1 goto missing
set /p RA2_ZIG_VERSION=<"%~dp0zig-version.tmp"
del /q "%~dp0zig-version.tmp"
if not "%RA2_ZIG_VERSION%"=="0.13.0" goto version_mismatch
rem One game runtime; normal CRT initialization.
"%ZIG%" rc /fo shared/source/dll_version.res shared/source/dll_version.rc
if errorlevel 1 goto fail
"%ZIG%" c++ -target x86-windows-gnu -std=c++17 -O2 -fno-exceptions -fno-rtti -c shared/source/game_entry_manager.cpp -o shared/source/game_entry_manager.o
if errorlevel 1 goto fail
"%ZIG%" rc /fo suite/source/app.res suite/source/app.rc
if errorlevel 1 goto fail
"%ZIG%" c++ -target x86-windows-msvc -O2 -fms-extensions -fno-stack-protector -c assist/source/guard.cpp -o assist/source/guard.o
if errorlevel 1 goto fail
"%ZIG%" c++ -target x86-windows-gnu -std=c++17 -O2 -fms-extensions -fasm-blocks -fno-stack-protector -fno-builtin -fno-exceptions -fno-rtti -c assist/source/bridge.cpp -o assist/source/bridge.o
if errorlevel 1 goto fail
"%ZIG%" c++ -target x86-windows-msvc -O2 -fms-extensions -fno-stack-protector -c cursor/source/resource_guard.cpp -o cursor/source/resource_guard.o
if errorlevel 1 goto fail
"%ZIG%" c++ -target x86-windows-gnu -O2 -DUNICODE -D_UNICODE -fms-extensions -fasm-blocks -fno-stack-protector -fno-builtin -fno-exceptions -fno-rtti -c cursor/source/hide.cpp -o cursor/source/hide.o
if errorlevel 1 goto fail
"%ZIG%" c++ -target x86-windows-gnu -std=c++17 -O2 -fms-extensions -fasm-blocks -fno-stack-protector -fno-builtin -fno-exceptions -fno-rtti -c game_runtime/source/runtime.cpp -o game_runtime/source/runtime.o
if errorlevel 1 goto fail
"%ZIG%" c++ -target x86-windows-gnu -shared game_runtime/source/runtime.o shared/source/game_entry_manager.o assist/source/bridge.o assist/source/guard.o cursor/source/hide.o cursor/source/resource_guard.o game_runtime/source/runtime.def shared/source/dll_version.res -o GameRuntime.dll -lkernel32 -luser32 -lgdi32 -lwinmm
if errorlevel 1 goto fail
"%ZIG%" c++ -target x86-windows-gnu -std=c++17 -O2 -DRA2_UNIFIED -DwWinMain=CursorEntry -Isuite/source -c cursor/source/main.cpp -o suite/source/cursor.o
if errorlevel 1 goto fail
"%ZIG%" c++ -target x86-windows-gnu -std=c++17 -O2 -DRA2_UNIFIED -DwWinMain=MonitorEntry -Isuite/source -c unit_monitor/source/main.cpp -o suite/source/unit_monitor.o
if errorlevel 1 goto fail
"%ZIG%" c++ -target x86-windows-gnu -std=c++17 -O2 -DRA2_UNIFIED -DwWinMain=TacticalEntry -Isuite/source -c assist/source/main.cpp -o suite/source/assist.o
if errorlevel 1 goto fail
"%ZIG%" c++ -target x86-windows-gnu -std=c++17 -O2 -municode suite/source/main.cpp suite/source/cursor.o suite/source/unit_monitor.o suite/source/assist.o suite/source/app.res -o RA2Toolbox.exe -Wl,--subsystem,windows -lgdiplus -lcomctl32 -luser32 -lgdi32 -lwinmm -lshell32 -lole32 -lcomdlg32
if errorlevel 1 goto fail
set "RA2_VERIFY_PYTHON=python"
if exist "runtime\python.exe" set "RA2_VERIFY_PYTHON=runtime\python.exe"
if exist "..\runtime\python.exe" set "RA2_VERIFY_PYTHON=..\runtime\python.exe"
"%RA2_VERIFY_PYTHON%" tools\verify_windows_outputs.py
if errorlevel 1 goto fail
echo Build verified. Replace RA2Toolbox.exe and GameRuntime.dll together.
if not defined RA2_NO_PAUSE pause
exit /b 0
:missing
echo Install Zig 0.13.0 and add its folder to PATH, or set ZIG_EXE to zig.exe.
if not defined RA2_NO_PAUSE pause
exit /b 1
:version_mismatch
echo Zig 0.13.0 required. Found %RA2_ZIG_VERSION%.
if not defined RA2_NO_PAUSE pause
exit /b 2
:fail
echo Build failed. Do not copy incomplete output files.
if not defined RA2_NO_PAUSE pause
exit /b 1
