#!/usr/bin/env bash
set -euo pipefail
probe_dir="$(cd "$(dirname "$0")" && pwd)"
probe_out="${1:?Usage: ZIG=/path/to/zig bash build.sh OUTPUT_DIRECTORY}"
mkdir -p "$probe_out"
probe_out="$(cd "$probe_out" && pwd)"
"${ZIG:-zig}" rc /fo "$probe_out/version.res" -- "$probe_dir/version.rc"
"${ZIG:-zig}" c++ -target x86_64-windows-gnu -std=c++17 -O2 -static -s -Wall -Wextra \
  "$probe_dir/main.cpp" "$probe_out/version.res" -lbcrypt -o "$probe_out/RA2MasterProbe-0.1.0.exe"
