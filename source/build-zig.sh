#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
: "${ZIG:=zig}"
version=$("$ZIG" version)
if [ "$version" != "0.13.0" ]; then
  echo "Zig 0.13.0 required; found $version" >&2
  exit 2
fi
# Use the normal DLL CRT startup: these links include the C/C++ runtime.
# A direct DllMain entry bypasses its initialization and static constructors.
rm -f BUILD-VERIFIED-v107.tag verification/BUILD-VERIFIED-v107.tag WINDOWS_BUILD_ARTIFACTS.json verification/WINDOWS_BUILD_ARTIFACTS.json
"$ZIG" rc /fo shared/source/dll_version.res shared/source/dll_version.rc
"$ZIG" c++ -target x86-windows-gnu -std=c++17 -O2 -fno-exceptions -fno-rtti -c shared/source/game_entry_manager.cpp -o shared/source/game_entry_manager.o
"$ZIG" rc /fo suite/source/app.res suite/source/app.rc
"$ZIG" c++ -target x86-windows-msvc -O2 -fms-extensions -fno-stack-protector -c assist/source/guard.cpp -o assist/source/guard.o
"$ZIG" c++ -target x86-windows-gnu -std=c++17 -O2 -fms-extensions -fasm-blocks -fno-stack-protector -fno-builtin -fno-exceptions -fno-rtti -c assist/source/bridge.cpp -o assist/source/bridge.o
"$ZIG" c++ -target x86-windows-msvc -O2 -fms-extensions -fno-stack-protector -c cursor/source/resource_guard.cpp -o cursor/source/resource_guard.o
"$ZIG" c++ -target x86-windows-gnu -O2 -DUNICODE -D_UNICODE -fms-extensions -fasm-blocks -fno-stack-protector -fno-builtin -fno-exceptions -fno-rtti -c cursor/source/hide.cpp -o cursor/source/hide.o
"$ZIG" c++ -target x86-windows-gnu -std=c++17 -O2 -fms-extensions -fasm-blocks -fno-stack-protector -fno-builtin -fno-exceptions -fno-rtti -c game_runtime/source/runtime.cpp -o game_runtime/source/runtime.o
"$ZIG" c++ -target x86-windows-gnu -shared game_runtime/source/runtime.o shared/source/game_entry_manager.o assist/source/bridge.o assist/source/guard.o cursor/source/hide.o cursor/source/resource_guard.o game_runtime/source/runtime.def shared/source/dll_version.res -o GameRuntime.dll -lkernel32 -luser32 -lgdi32 -lwinmm
for entry in Cursor Monitor Tactical;do
  case "$entry" in Cursor) module=cursor;;Monitor) module=unit_monitor;;Tactical) module=assist;;esac
  "$ZIG" c++ -target x86-windows-gnu -std=c++17 -O2 -DRA2_UNIFIED -DwWinMain="${entry}Entry" -Isuite/source -c "$module/source/main.cpp" -o "suite/source/$module.o"
done
"$ZIG" c++ -target x86-windows-gnu -std=c++17 -O2 -municode suite/source/main.cpp suite/source/cursor.o suite/source/unit_monitor.o suite/source/assist.o suite/source/app.res -o RA2Toolbox.exe -Wl,--subsystem,windows -lgdiplus -lcomctl32 -luser32 -lgdi32 -lwinmm -lshell32 -lole32 -lcomdlg32

