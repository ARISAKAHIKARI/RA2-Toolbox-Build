#!/usr/bin/env bash
# Portable smoke compile of the self-trampoline entry stub only.
set -euo pipefail
cd "$(dirname "$0")"
clang++ -target i686-w64-windows-gnu -O2 -fms-extensions -fasm-blocks -c frame_observer_asm_smoke.cpp -o /tmp/ra2-frame-observer.o
file /tmp/ra2-frame-observer.o
