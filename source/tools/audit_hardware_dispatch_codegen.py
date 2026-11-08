#!/usr/bin/env python3
"""Audit the actual Win32 Runtime objects; this does not execute Windows or #DB."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def symbols(path):
    text = subprocess.check_output(['objdump', '-t', str(path)], text=True)
    return [line.split()[-1] for line in text.splitlines() if '(ty   20)' in line]


def function(path, symbol):
    text = subprocess.check_output(['objdump', '-dr', '--disassemble=' + symbol, str(path)], text=True)
    start = re.search(r'^[ \t]*[0-9a-f]+ <' + re.escape(symbol) + r'>:\n', text, re.M)
    if not start:
        raise ValueError('Missing function: ' + symbol)
    body = text[start.start():]
    end = re.search(r'\n(?:[ \t]*[0-9a-f]+ <[^\n]+>:\n|Disassembly of section)', body)
    if end:
        body = body[:end.start()]
    instructions = []
    for line in body.splitlines():
        match = re.match(r'^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2}\s+)+\s*([a-z][a-z0-9]+)\s*(.*)$', line)
        if match:
            instructions.append({'address': match[1], 'mnemonic': match[2], 'operands': match[3]})
    return body + '\n', instructions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before-object', required=True, type=Path)
    parser.add_argument('--after-object', required=True, type=Path)
    parser.add_argument('--preflight-receipt', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    receipt = json.loads(args.preflight_receipt.read_text())
    assert receipt['exit_code'] == 0 and receipt['source_inputs_unchanged']
    assert receipt['object_sha256'] == sha(args.after_object)
    assert all(sha(Path(path)) == digest for path, digest in receipt['source_hashes'].items())
    records = {}
    for name, path in [('before', args.before_object), ('after', args.after_object)]:
        matches = [s for s in symbols(path) if s.startswith('__ZN6assist13controlparams5worldI')]
        assert len(matches) == 1, (name, matches)
        body, instructions = function(path, matches[0])
        asm = out / (name + '-world.asm')
        asm.write_text(body)
        calls = [i for i in instructions if i['mnemonic'].startswith('call')]
        records[name] = dict(object=str(path.resolve()), object_sha256=sha(path), symbol=matches[0],
                             static_instructions=len(instructions), calls=calls,
                             indirect_calls=sum('*' in i['operands'] for i in calls),
                             assembly={'path': str(asm), 'sha256': sha(asm)})
    assert records['before']['indirect_calls'] == 10
    assert records['after']['indirect_calls'] == 0 and not records['after']['calls']
    callback, _ = function(args.after_object, '_AssistTechUiParameter')
    callback_path = out / 'after-callback.asm'
    callback_path.write_text(callback)
    lines = callback.splitlines()
    masks = [i for i, line in enumerate(lines) if 'fffff72a' in line]
    assert len(masks) == 1
    i = masks[0]
    constant_path = lines[max(0, i - 2):i + 4]
    assert '$0x2,' in constant_path[0] and 'jne' in constant_path[1]
    assert any('$0x11,' in line and 'or' in line for line in constant_path)
    assert not any(re.search(r'\bcall\b', line) for line in constant_path)
    assert 'compareFlagsEjjj' in callback  # The dynamic tag3 path remains.
    report = dict(pass_=True, baseline_commit='cc3f0117c233284cec1963a11c26d637d55817fe',
                  scope=__doc__, source_sha256=receipt['source_hashes'],
                  audit_source_sha256={str(Path(__file__).resolve()): sha(Path(__file__).resolve())},
                  objects=records, tag2_constant_flags_assembly=constant_path,
                  tag3_dynamic_comparison_retained=True,
                  callback_assembly={'path': str(callback_path), 'sha256': sha(callback_path)},
                  preflight_receipt={'path': str(args.preflight_receipt.resolve()), 'sha256': sha(args.preflight_receipt)},
                  windows_executed=False, game_tested=False, fps_measured=False)
    (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'world_indirect_calls': [records[n]['indirect_calls'] for n in ('before', 'after')],
                      'world_static_instructions': [records[n]['static_instructions'] for n in ('before', 'after')],
                      'tag2_constant_flags_verified': True}))


if __name__ == '__main__':
    main()
