"""Verify the real pre.3/current control branches and their intentional direct-Yes semantic change."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def branch(text):
    start = text.index('if(tag==4){')
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end] + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cxx', default='g++')
    parser.add_argument('--baseline-ref', default='b9e7111391f9ffcbf20d3a1baf785498a8792155')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    source = here.parent / 'source'
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    baseline = subprocess.check_output(['git', 'show', args.baseline_ref + ':source/assist/source/tech_ui_breakpoint_bridge.h'], cwd=repo, text=True)
    old = out / 'baseline-control.inc'
    new = out / 'current-control.inc'
    old.write_text(branch(baseline))
    new.write_text(branch((source / 'tech_ui_breakpoint_bridge.h').read_text()))
    digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    paths = [source / name for name in ('tech_ui_breakpoint_bridge.h', 'control_parameter_policy.h', 'control_mode_policy.h', 'tech_ui_breakpoint_hot.h')]
    paths += [repo / 'source/shared/source' / name for name in ('master_control_policy.h', 'patch_context.h', 'debug_entry_types.h', 'debug_entry_policy.h', 'debug_entry_catalog.h')]
    paths += [here / 'hardware_control_reader.cpp', Path(__file__).resolve()]
    hashes = {str(p): digest(p) for p in paths}
    exe = out / 'hardware-control-reader'
    command = [args.cxx, '-std=c++17', '-O2', '-UNDEBUG', '-DBASELINE_CONTROL_BRANCH="' + str(old) + '"', '-DCURRENT_CONTROL_BRANCH="' + str(new) + '"', str(here / 'hardware_control_reader.cpp'), '-o', str(exe)]
    build = subprocess.run(command, capture_output=True, text=True)
    (out / 'build.log').write_text(build.stdout + build.stderr)
    if build.returncode:
        raise RuntimeError(build.stdout + build.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True, check=True)
    (out / 'run.log').write_text(run.stdout + run.stderr)
    result = json.loads(run.stdout)
    assert result['pass'] and hashes == {str(p): digest(p) for p in paths}
    report = dict(pass_=True, scope=__doc__, baseline_ref=args.baseline_ref, command=command, source_sha256=hashes,
                  baseline_branch_sha256=digest(old), current_branch_sha256=digest(new), result=result)
    (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(run.stdout, end='')


if __name__ == '__main__':
    main()
