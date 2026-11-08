#!/usr/bin/env python3
"""Audit actual pre.3/current Win32 Runtime control routing, without running Windows."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

from audit_hardware_dispatch_codegen import function, symbols


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def control_branch(body):
    """The tag-4 block starts at its unique owner counter and ends in a jump."""
    lines = body.splitlines()
    locations = [i for i, line in enumerate(lines) if 'techparametersL17controlExceptionsE' in line]
    assert len(locations) == 1, locations
    start = locations[0] - 1
    assert re.search(r'\bincl?\b', lines[start]), lines[start]
    end = start + 1
    while end < len(lines) and not re.search(r'\bjmp\s', lines[end]):
        end += 1
    assert end < len(lines), 'Missing terminal control-route jump'
    branch = '\n'.join(lines[start:end + 1]) + '\n'
    for literal in ('0x50b710', '0x50b716', '0x6339bf', '0x637e45'):
        assert literal in branch, (literal, branch)
    instructions = [line for line in branch.splitlines()
                    if re.match(r'^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2}\s+)+\s*[a-z][a-z0-9]+\s', line)]
    calls = [line for line in branch.splitlines() if re.search(r'\bcall\s', line)]
    return branch, len(instructions), calls


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before-object', type=Path, required=True)
    p.add_argument('--after-object', type=Path, required=True)
    p.add_argument('--preflight-receipt', type=Path, required=True)
    p.add_argument('--guard-object', type=Path, required=True)
    p.add_argument('--guard-receipt', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    receipt = json.loads(a.preflight_receipt.read_text())
    assert receipt['exit_code'] == 0 and receipt['source_inputs_unchanged']
    assert receipt['object_sha256'] == sha(a.after_object)
    assert all(sha(path) == digest for path, digest in receipt['source_hashes'].items())
    guard_receipt = json.loads(a.guard_receipt.read_text())
    assert guard_receipt['exit_code'] == 0 and guard_receipt['source_inputs_unchanged']
    assert guard_receipt['object_sha256'] == sha(a.guard_object)
    assert all(sha(path) == digest for path, digest in guard_receipt['source_hashes'].items())
    records = {}
    for name, obj in (('before', a.before_object), ('after', a.after_object)):
        body, _ = function(obj, '_AssistTechUiParameter')
        branch, count, calls = control_branch(body)
        path = a.out / (name + '-control.asm')
        path.write_text(branch)
        full = a.out / (name + '-callback.asm')
        full.write_text(body)
        records[name] = dict(object=str(obj.resolve()), object_sha256=sha(obj),
                             control_static_instructions=count, control_calls=calls,
                             world_or_house_calls=sum(bool(re.search(r'controlparams[56](world|house)', line)) for line in branch.splitlines()),
                             assembly={'path': str(path.resolve()), 'sha256': sha(path)},
                             full_callback={'path': str(full.resolve()), 'sha256': sha(full)})
        classify_symbols = [symbol for symbol in symbols(obj) if 'Service8classify' in symbol]
        assert len(classify_symbols) == 1, classify_symbols
        classifier, classifier_instructions = function(obj, classify_symbols[0])
        classifier_path = a.out / (name + '-classify.asm')
        classifier_path.write_text(classifier)
        records[name]['classifier'] = dict(static_instructions=len(classifier_instructions),
            variable_shifts=sum(i['mnemonic'] in ('shl', 'shr', 'sal', 'sar') and '%cl' in i['operands'] for i in classifier_instructions),
            assembly={'path': str(classifier_path.resolve()), 'sha256': sha(classifier_path)})
    assert records['before']['world_or_house_calls'] == 2
    assert records['after']['world_or_house_calls'] == 0 and not records['after']['control_calls']
    assert records['after']['control_static_instructions'] < records['before']['control_static_instructions']
    assert records['before']['classifier']['variable_shifts'] == 8
    assert records['after']['classifier']['variable_shifts'] == 2
    cold, cold_instructions = function(a.after_object, '_AssistControlWorldEligible')
    cold_path = a.out / 'after-cold-world-eligibility.asm'
    cold_path.write_text(cold)
    assert '0xa83d4c' in cold or 'controlparams5world' in cold
    assert 'ReadProcessMemory' not in cold
    guard = subprocess.check_output(['objdump', '-dr', '--disassemble=_GuardedControlWorldEligible', str(a.guard_object)], text=True)
    assert '_AssistControlWorldEligible' in guard and '__except_handler3' in guard
    assert '__imp__GetLastError@0' in guard and guard.count('__imp__SetLastError@4') == 2
    assert 'GuardedControlWorldEligible@4HA' in guard and '?filt$0@0@GuardedControlWorldEligible@@' in guard
    guard_path = a.out / 'after-cold-guard.asm'
    guard_path.write_text(guard)
    report = dict(pass_=True, scope=__doc__, baseline_commit='b9e7111391f9ffcbf20d3a1baf785498a8792155',
                  source_sha256=dict(receipt['source_hashes'], **guard_receipt['source_hashes']),
                  audit_source_sha256={str(Path(__file__).resolve()): sha(__file__),
                                       str(Path(__file__).with_name('audit_hardware_dispatch_codegen.py').resolve()):
                                       sha(Path(__file__).with_name('audit_hardware_dispatch_codegen.py'))},
                  objects=records,
                  cold_eligibility={'path': str(cold_path.resolve()), 'sha256': sha(cold_path),
                                    'static_instructions': len(cold_instructions)},
                  preflight_receipt={'path': str(a.preflight_receipt.resolve()), 'sha256': sha(a.preflight_receipt)},
                  cold_guard={'object': str(a.guard_object.resolve()), 'object_sha256': sha(a.guard_object),
                              'receipt': {'path': str(a.guard_receipt.resolve()), 'sha256': sha(a.guard_receipt)},
                              'assembly': {'path': str(guard_path.resolve()), 'sha256': sha(guard_path)},
                              'native_seh_and_last_error_restore': True},
                  windows_executed=False, game_tested=False, fps_measured=False)
    (a.out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'control_static_instructions': [records[n]['control_static_instructions'] for n in ('before', 'after')],
                      'control_world_house_calls': [2, 0], 'control_direct_leaf_routing': True}))


if __name__ == '__main__':
    main()
