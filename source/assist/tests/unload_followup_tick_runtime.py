"""Compile and execute the actual production smartUnloadTick with faulted live reads.

Synthetic C++ memory and native seams are explicit; this does not execute RA2.
"""
from pathlib import Path
import argparse
import subprocess

here = Path(__file__).resolve().parent
source = here.parent / 'source' / 'native_orders_bridge.h'
parser = argparse.ArgumentParser()
parser.add_argument('--out', type=Path, required=True)
a = parser.parse_args()
a.out.mkdir(parents=True, exist_ok=True)
text = source.read_text()
signature = 'static void smartUnloadTick(uint32_t house,int frame,int&budget){'
start = text.index(signature)
at = text.index('{', start)
end, depth = at + 1, 1
while depth:
    depth += (text[end] == '{') - (text[end] == '}')
    end += 1
extracted = a.out / 'smart_unload_tick.inc'
extracted.write_text(text[start:end] + '\n')
exe = a.out / 'fixture'
cmd = ['g++', '-std=c++17', '-O1', '-Wall', '-Wextra', '-UNDEBUG',
       '-DUNLOAD_TICK_HEADER="' + str(extracted.resolve()) + '"',
       str(here / 'unload_followup_tick_runtime.cpp'), '-o', str(exe)]
for command in (cmd, [str(exe)]):
    run = subprocess.run(command, capture_output=True, text=True, timeout=120)
    print(run.stdout + run.stderr, end='', flush=True)
    if run.returncode:
        raise SystemExit(run.returncode)
