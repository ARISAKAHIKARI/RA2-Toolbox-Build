#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
: "${ZIG:=zig}"
version=$("$ZIG" version)
if [ "$version" != "0.13.0" ]; then
  echo "Zig 0.13.0 required; found $version" >&2
  exit 2
fi
# Only the external UI/reader/renderer becomes x64. The DLL and hook broker
# execute in or bind to the 32-bit game and must remain x86.
build_dir=${RA2_OUTPUT_DIR:-"$PWD/build-x64"}
mkdir -p "$build_dir"
build_dir=$(cd "$build_dir" && pwd)
if [ "$build_dir" = "$PWD" ]; then
  echo "The mixed-architecture build requires a separate RA2_OUTPUT_DIR" >&2
  exit 2
fi
runtime_dir="$build_dir/obj/runtime-x86"
host_dir="$build_dir/obj/host-x64"
mkdir -p "$runtime_dir" "$host_dir"
rm -f "$build_dir/BUILD-VERIFIED-v130.tag" "$build_dir/WINDOWS_BUILD_ARTIFACTS.json"
# Git Bash must not rewrite RC's /fo and /d switches as Windows paths.
# Every resource input below is relative to this source directory; convert
# only the absolute output path before disabling MSYS argument conversion.
rc(){
  local output=$1
  shift
  if command -v cygpath >/dev/null 2>&1; then output=$(cygpath -m "$output"); fi
  MSYS2_ARG_CONV_EXCL='*' "$ZIG" rc /fo "$output" "$@"
}
rc "$runtime_dir/dll_version.res" shared/source/dll_version.rc
"$ZIG" c++ -target x86-windows-gnu -std=c++17 -O2 -fno-exceptions -fno-rtti -c shared/source/game_entry_manager.cpp -o "$runtime_dir/game_entry_manager.o"
"$ZIG" c++ -target x86-windows-msvc -O2 -fms-extensions -fno-stack-protector -c assist/source/guard.cpp -o "$runtime_dir/guard.o"
"$ZIG" c++ -target x86-windows-gnu -std=c++17 -O2 -fms-extensions -fasm-blocks -fno-stack-protector -fno-builtin -fno-exceptions -fno-rtti -c assist/source/bridge.cpp -o "$runtime_dir/bridge.o"
"$ZIG" c++ -target x86-windows-msvc -O2 -fms-extensions -fno-stack-protector -c cursor/source/resource_guard.cpp -o "$runtime_dir/resource_guard.o"
"$ZIG" c++ -target x86-windows-gnu -O2 -DUNICODE -D_UNICODE -fms-extensions -fasm-blocks -fno-stack-protector -fno-builtin -fno-exceptions -fno-rtti -c cursor/source/hide.cpp -o "$runtime_dir/hide.o"
"$ZIG" c++ -target x86-windows-gnu -std=c++17 -O2 -fms-extensions -fasm-blocks -fno-stack-protector -fno-builtin -fno-exceptions -fno-rtti -c game_runtime/source/runtime.cpp -o "$runtime_dir/runtime.o"
"$ZIG" c++ -target x86-windows-gnu -shared "$runtime_dir/runtime.o" "$runtime_dir/game_entry_manager.o" "$runtime_dir/bridge.o" "$runtime_dir/guard.o" "$runtime_dir/hide.o" "$runtime_dir/resource_guard.o" game_runtime/source/runtime.def "$runtime_dir/dll_version.res" -o "$build_dir/GameRuntime.dll" -lkernel32 -luser32 -lgdi32 -lwinmm
rc "$runtime_dir/broker.res" runtime_broker/source/app.rc
"$ZIG" c++ -target x86-windows-gnu -std=c++17 -O2 -municode runtime_broker/source/main.cpp "$runtime_dir/broker.res" -o "$build_dir/GameRuntimeHost32.exe" -Wl,--subsystem,windows -lkernel32 -luser32
rc "$host_dir/app.res" /d RA2_HOST_X64=1 suite/source/app.rc
"$ZIG" c++ -target x86_64-windows-msvc -O2 -fms-extensions -fno-stack-protector -DRA2_GPU_FAULT_GUARD -c shared/source/gpu_fault_guard.cpp -o "$host_dir/gpu_fault_guard.o"
"$ZIG" c++ -target x86_64-windows-gnu -std=c++17 -O2 -fno-exceptions -fno-rtti -c third_party/xbrz/xbrz.cpp -o "$host_dir/xbrz.o"
for entry in Cursor Monitor Tactical; do
  case "$entry" in Cursor) module=cursor;; Monitor) module=unit_monitor;; Tactical) module=assist;; esac
  "$ZIG" c++ -target x86_64-windows-gnu -std=c++17 -O2 -DRA2_UNIFIED -DRA2_GPU_FAULT_GUARD -DwWinMain="${entry}Entry" -Isuite/source -c "$module/source/main.cpp" -o "$host_dir/$module.o"
done
"$ZIG" c++ -target x86_64-windows-gnu -std=c++17 -O2 -municode suite/source/main.cpp "$host_dir/cursor.o" "$host_dir/unit_monitor.o" "$host_dir/assist.o" "$host_dir/xbrz.o" "$host_dir/gpu_fault_guard.o" "$host_dir/app.res" -o "$build_dir/RA2Toolbox.exe" -Wl,--subsystem,windows -lgdiplus -lcomctl32 -luser32 -lgdi32 -lwinmm -lshell32 -lole32 -lcomdlg32
cp BUILD_SOURCE_MANIFEST.json "$build_dir/BUILD_SOURCE_MANIFEST.json"
printf '%s\n' '{"host":"x64","runtime":"x86","broker":"x86","assist_protocol":90,"cursor_protocol":155,"runtime_protocol":4,"game_tested":false}' > "$build_dir/BUILD_TARGET.json"
