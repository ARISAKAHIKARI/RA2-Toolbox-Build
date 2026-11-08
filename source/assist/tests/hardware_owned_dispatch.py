"""Compare complete production Service dispatch against a fixed source baseline.

The native comparison replaces Win32 delivery and SEH with controlled boundaries;
it executes the real Service, not a reconstructed dispatch algorithm. Separately
compile/link both headers with the production Win32 x86 -O2 flags and actual SEH
boundary, retaining assembly for the entry/body/cleanup. Neither is Windows DR
execution, a complete feature-callback benchmark, or a game/FPS measurement.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess

from breakpoint_windows_build import build_guard

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline-core', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--zig', type=Path, required=True)
p.add_argument('--cxx', default='g++')
p.add_argument('--iterations', type=int, default=3000000)
p.add_argument('--rounds', type=int, default=7)
a = p.parse_args()
assert 0 < a.iterations <= 20000000 and a.rounds >= 3
here = Path(__file__).resolve().parent
out = a.out.resolve(); out.mkdir(parents=True, exist_ok=True)
cores = {'before': a.baseline_core.resolve(), 'after': here.parent/'source/tech_ui_breakpoint_core.h'}
sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
source_hashes = {name: sha(core) for name, core in cores.items()}
native = {}
for name, core in cores.items():
    directory = out/(name+'-native')
    command = ['python', str(here/'breakpoint_owner_rendezvous.py'), '--core', str(core), '--out', str(directory), '--cxx', a.cxx]
    subprocess.run(command, check=True, capture_output=True, text=True)
    native[name] = json.loads((directory/'result.json').read_text())

# Interleave on one allowed CPU where affinity is available. Do not include
# process startup, installation/witness/cleanup, clocks or checks in the timer.
cpu = min(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else None
def run_native(name, iterations):
    binary = out/(name+'-native')/('owner-rendezvous.exe' if os.name == 'nt' else 'owner-rendezvous')
    options = {'preexec_fn': lambda: os.sched_setaffinity(0, {cpu})} if cpu is not None else {}
    run = subprocess.run([str(binary), '--benchmark', str(iterations)], check=True, capture_output=True, text=True, **options)
    result = json.loads(run.stdout)
    assert result['iterations'] == iterations
    assert result['scopes'] == result['cleanups'] == result['callbacks'] == result['thread_id_calls'] == iterations
    assert result['clock_calls'] == result['atomic_calls'] == result['maintenance_traps'] == 0
    return result
for name in cores: run_native(name, min(a.iterations, 250000))
rounds = []
for round_index in range(a.rounds):
    order = ['before', 'after'] if round_index % 2 == 0 else ['after', 'before']
    rows = {name: run_native(name, a.iterations) for name in order}
    rounds.append({'order': order, **rows})

seh = build_guard(a.zig, out/'win32-seh')
machine = {}
for name, core in cores.items():
    directory = out/(name+'-win32'); directory.mkdir(exist_ok=True)
    probe = directory/'probe.cpp'
    probe.write_text('#include <windows.h>\n#include <type_traits>\n#define private public\n#include "'+str(core)+'"\n#undef private\n'
                     'using Service=assist::techbp::Service;\n'
                     'extern "C" __declspec(dllexport) void ProbeInstance(Service*s){Service::instance()=s;}\n'
                     'extern "C" __declspec(dllexport) void* ProbeReceiver(){return reinterpret_cast<void*>(&Service::vectored);}\n')
    obj = directory/'probe.o'; dll = directory/'probe.dll'
    commands = [[str(a.zig.resolve()), 'c++', '-target', 'x86-windows-gnu', '-std=c++17', '-O2', '-fms-extensions', '-fasm-blocks', '-fno-stack-protector', '-fno-builtin', '-fno-exceptions', '-fno-rtti', '-c', str(probe), '-o', str(obj)],
                [str(a.zig.resolve()), 'c++', '-target', 'x86-windows-gnu', '-shared', str(obj), str(seh), '-o', str(dll)]]
    for command, log in zip(commands, ['compile.log', 'link.log']):
        run = subprocess.run(command, capture_output=True, text=True)
        (directory/log).write_text(run.stdout+run.stderr)
        if run.returncode: raise RuntimeError(run.stdout+run.stderr)
    symbols = [line.split()[-1] for line in subprocess.check_output(['nm', str(obj)], text=True).splitlines() if ' T ' in line]
    selected = {}
    for purpose, match in [('entry', 'Service8vectored'), ('body', '6invokeEPv'), ('cleanup', '6finishEPv'), ('dispatch', 'Service8dispatch')]:
        candidates = [s for s in symbols if match in s and (purpose in ('entry', 'dispatch') or 'DispatchInvocation' in s or 'Service8vectored' in s)]
        if purpose == 'entry': candidates = [s for s in candidates if s.endswith('@4')]
        assert len(candidates) == 1, (name, purpose, candidates)
        symbol = candidates[0]
        assembly = subprocess.check_output(['objdump', '-dr', '--disassemble='+symbol, str(obj)], text=True)
        instructions = []
        for line in assembly.splitlines():
            m = re.match(r'^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2}\s+)+\s*([a-z][a-z0-9]*\s+.*|ret\S*.*|push\S*.*|pop\S*.*)$', line)
            if m: instructions.append(m[1].strip())
        assert instructions, (name, purpose)
        (directory/(purpose+'.asm')).write_text(assembly)
        selected[purpose] = {'symbol': symbol, 'instruction_count': len(instructions), 'instructions': instructions}
    machine[name] = {'commands': commands, 'object_sha256': sha(obj), 'dll_sha256': sha(dll), 'functions': selected}

assert source_hashes == {name: sha(core) for name, core in cores.items()}
medians = {name: statistics.median(r[name]['elapsed_ns']/a.iterations for r in rounds) for name in cores}
report = {'pass': True, 'scope': __doc__, 'source_sha256': source_hashes,
          'fixture_sha256': {str(path): sha(path) for path in [Path(__file__), here/'breakpoint_owner_rendezvous.cpp', here/'breakpoint_owner_rendezvous.py', here/'breakpoint_windows_build.py']},
          'functional': native, 'native_affinity_cpu': cpu, 'native_rounds': rounds, 'native_median_ns_per_dispatch': medians,
          'win32': machine, 'windows_executed': False, 'game_executed': False}
(out/'comparison.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps({'pass': True, 'native_median_ns_per_dispatch': medians,
                  'win32_function_instruction_counts': {name: {key: row['instruction_count'] for key, row in info['functions'].items()} for name, info in machine.items()},
                  'receipt': str(out/'comparison.json')}, indent=2))
