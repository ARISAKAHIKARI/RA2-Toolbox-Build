"""Exercise extracted production control admission/publication/maintenance and callback with explicit memory, Service and exception models."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def block(text, marker, semicolon=False):
    start = text.index(marker)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    if semicolon:
        assert text[end] == ';'
        end += 1
    return text[start:end] + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--cxx', default='g++')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    source = here.parent / 'source'
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    text = (source / 'tech_ui_breakpoint_bridge.h').read_text()
    structures = ['struct TypeIdentity', 'struct EnhancedIdentity', 'struct Policy', 'struct FailureRetry']
    helpers = ['static bool controlCompatible(', 'static bool healthy(', 'static bool ready(', 'static uint32_t enhancementTags(',
               'static bool groupOff(', 'static bool retryFailure(', 'static void syncDebugBreakpointPolicy(',
               'static bool captureWorld(', 'static bool refreshControlEligibility(', 'static bool configure(']
    lifecycle = ['static void maintainTechParameters(', 'static bool controlParametersRetired(',
                 'static bool restoreControlParameters(', 'static int requestControlParameters(']
    pieces = {
        'control-types.inc': ''.join(block(text, item, True) for item in structures),
        'control-helpers.inc': ''.join(block(text, item) for item in helpers),
        'control-cold.inc': block(text, 'extern "C" int __cdecl AssistControlWorldEligible('),
        'control-lifecycle.inc': ''.join(block(text, item) for item in lifecycle),
        'control-hot.inc': block(text, 'if(tag==4){'),
    }
    for name, value in pieces.items():
        (out / name).write_text(value)
    source_names = ['tech_ui_breakpoint_bridge.h', 'tech_ui_breakpoint_guard.h', 'control_parameter_policy.h',
                    'control_mode_policy.h', 'tech_ui_breakpoint_hot.h', 'tech_parameter_lookup.h', 'exception_policy.h',
                    'tech_enhancement.h', 'tech_enhancement_parameter_policy.h', 'technology_mode_policy.h',
                    'map_visibility_parameter_policy.h', 'single_player_fps.h', 'event_delivery.h']
    paths = [source / name for name in source_names]
    paths += [repo / 'source/shared/source' / name for name in ('master_control_policy.h', 'patch_context.h', 'debug_entry_types.h', 'debug_entry_policy.h', 'debug_entry_catalog.h')]
    paths += [here / 'control_publication.cpp', Path(__file__).resolve()]
    digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    hashes = {str(path): digest(path) for path in paths}
    exe = out / 'control-publication'
    command = [args.cxx, '-std=c++17', '-O2', '-UNDEBUG', '-I', str(out), str(here / 'control_publication.cpp'), '-o', str(exe)]
    build = subprocess.run(command, capture_output=True, text=True)
    (out / 'build.log').write_text(build.stdout + build.stderr)
    if build.returncode:
        raise RuntimeError(build.stdout + build.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    (out / 'run.log').write_text(run.stdout + run.stderr)
    if run.returncode:
        raise RuntimeError(run.stdout + run.stderr)
    result = json.loads(run.stdout)
    stable = hashes == {str(path): digest(path) for path in paths}
    report = {'pass': result['pass'] and stable, 'scope': __doc__, 'command': command,
              'source_sha256': hashes, 'source_hash_stable': stable,
              'extracted_functions': helpers + lifecycle + ['AssistControlWorldEligible', 'AssistTechUiParameter tag4'],
              'extracted_sha256': {name: digest(out / name) for name in pieces},
              'modeled_dependencies': ['Service DR lease/transaction and clock', 'game memory', 'portable noReportAccess fault routing and LastError'],
              'real_windows_seh_executed': False, 'game_process_executed': False, 'result': result}
    (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(run.stdout, end='')
    assert report['pass']


if __name__ == '__main__':
    main()
