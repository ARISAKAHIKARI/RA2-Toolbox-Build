"""Compare complete production Service dispatch against a fixed source baseline.

The native comparison replaces Win32 delivery and SEH with controlled boundaries;
it executes the real Service, not a reconstructed dispatch algorithm. Separately
compile/link both headers with the production Win32 x86 -O2 flags and actual SEH
boundary, retaining assembly for classification and entry/body/cleanup. Neither is Windows DR
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
p.add_argument('--baseline-ref', default='b9e7111391f9ffcbf20d3a1baf785498a8792155')
p.add_argument('--baseline-core', type=Path, help='Optional existing core; it must match the fixed baseline commit')
p.add_argument('--baseline-object', type=Path, help='Actual baseline Runtime bridge.o for per-function machine-code comparison')
p.add_argument('--out', type=Path, required=True)
p.add_argument('--zig', type=Path, required=True)
p.add_argument('--cxx', default='g++')
p.add_argument('--iterations', type=int, default=3000000)
p.add_argument('--rounds', type=int, default=7)
a = p.parse_args()
assert 0 < a.iterations <= 20000000 and a.rounds >= 3
here = Path(__file__).resolve().parent
repo = here.parents[2]
out = a.out.resolve(); out.mkdir(parents=True, exist_ok=True)
sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
baseline_commit = subprocess.check_output(['git', 'rev-parse', a.baseline_ref+'^{commit}'], cwd=repo, text=True).strip()
baseline_paths = ['source/assist/source/tech_ui_breakpoint_core.h',
                  'source/assist/source/tech_ui_breakpoint_hot.h',
                  'source/assist/source/tech_ui_breakpoint_rendezvous_guard.h',
                  'source/shared/source/runtime_seh_scope.h', 'source/shared/source/hardware_slot_budget.h']
baseline_sources = {}
for relative in baseline_paths:
    content = subprocess.check_output(['git', 'show', baseline_commit+':'+relative], cwd=repo)
    baseline_sources[relative] = hashlib.sha256(content).hexdigest()
    if not a.baseline_core:
        target = out/'baseline-source'/relative
        target.parent.mkdir(parents=True, exist_ok=True); target.write_bytes(content)
baseline_core = a.baseline_core.resolve() if a.baseline_core else out/'baseline-source'/baseline_paths[0]
baseline_root = baseline_core.parents[3]
for relative, expected in baseline_sources.items():
    assert sha(baseline_root/relative) == expected, ('baseline source mismatch', relative)
cores = {'before': baseline_core, 'after': here.parent/'source/tech_ui_breakpoint_core.h'}
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
boundary = out/'win32-seh/rendezvous-production-seh.cpp'
scope_definition = boundary.read_text().split('#include', 1)[0].rstrip()
scope_header = here.parent/'source/tech_ui_breakpoint_rendezvous_guard.h'
scope_header_sha = sha(scope_header)
def disassemble(obj, symbol):
    assembly = subprocess.check_output(['objdump', '-dr', '--no-show-raw-insn', '--disassemble='+symbol, str(obj)], text=True)
    lines = assembly.splitlines()
    start = next(i for i, line in enumerate(lines) if '<'+symbol+'>:' in line)
    block = []
    for line in lines[start:]:
        if block and (line.startswith('Disassembly of section') or re.match(r'^[0-9a-f]+ <', line)): break
        block.append(line)
    instructions = []
    for line in block:
        match = re.match(r'^\s*[0-9a-f]+:\t([a-z].*)$', line)
        if match: instructions.append(match[1].strip())
    assert instructions, (str(obj), symbol)
    return '\n'.join(block).strip()+'\n', instructions

def normalized(instructions):
    return [re.sub(r'\s+<.*$', '', line) for line in instructions]

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
    for purpose, match in [('entry', 'Service8vectored'), ('classify', 'Service8classify'), ('body', '6invokeEPv'), ('cleanup', '6finishEPv'), ('dispatch', 'Service8dispatch')]:
        candidates = [s for s in symbols if match in s and (purpose in ('entry', 'classify', 'dispatch') or 'DispatchInvocation' in s or 'Service8vectored' in s)]
        if purpose == 'entry': candidates = [s for s in candidates if s.endswith('@4')]
        assert len(candidates) == 1, (name, purpose, candidates)
        symbol = candidates[0]
        assembly, instructions = disassemble(obj, symbol)
        (directory/(purpose+'.asm')).write_text(assembly)
        selected[purpose] = {'symbol': symbol, 'instruction_count': len(instructions), 'instructions': instructions,
                             'variable_shift_count': sum('%cl,' in line and line.split()[0] in ('shl', 'shr', 'sar') for line in instructions)}
    machine[name] = {'commands': commands, 'object_sha256': sha(obj), 'dll_sha256': sha(dll), 'functions': selected}

crosscheck = None
if a.baseline_object:
    functions = {}
    for purpose, reference in machine['before']['functions'].items():
        assembly, instructions = disassemble(a.baseline_object, reference['symbol'])
        path = out/('actual-baseline-'+purpose+'.asm'); path.write_text(assembly)
        equal = normalized(instructions) == normalized(reference['instructions'])
        assert equal, ('actual baseline object differs', purpose)
        functions[purpose] = {'instruction_count': len(instructions), 'equal_to_pinned_baseline': equal,
                              'assembly': {'path': str(path), 'sha256': sha(path)}}
    crosscheck = {'object': str(a.baseline_object.resolve()), 'object_sha256': sha(a.baseline_object),
                  'baseline_commit': baseline_commit, 'functions': functions}

assert source_hashes == {name: sha(core) for name, core in cores.items()}
assert sha(scope_header) == scope_header_sha
assert scope_definition in (here.parent/'source/guard.cpp').read_text()
medians = {name: statistics.median(r[name]['elapsed_ns']/a.iterations for r in rounds) for name in cores}
report = {'pass': True, 'scope': __doc__, 'baseline_commit': baseline_commit, 'baseline_source_sha256': baseline_sources,
          'source_sha256': source_hashes, 'actual_baseline_object': crosscheck,
          'fixture_sha256': {str(path): sha(path) for path in [Path(__file__), here/'breakpoint_owner_rendezvous.cpp', here/'breakpoint_owner_rendezvous.py', here/'breakpoint_windows_build.py']},
          'seh_boundary': {'source': str(boundary), 'source_sha256': sha(boundary),
                           'scope_definition_sha256': hashlib.sha256(scope_definition.encode()).hexdigest(),
                           'header': str(scope_header), 'header_sha256': scope_header_sha,
                           'scope_definition_still_matches_production': True},
          'functional': native, 'native_affinity_cpu': cpu, 'native_rounds': rounds, 'native_median_ns_per_dispatch': medians,
          'win32': machine, 'windows_executed': False, 'game_executed': False}
(out/'comparison.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps({'pass': True, 'native_median_ns_per_dispatch': medians,
                  'win32_function_instruction_counts': {name: {key: row['instruction_count'] for key, row in info['functions'].items()} for name, info in machine.items()},
                  'receipt': str(out/'comparison.json')}, indent=2))
