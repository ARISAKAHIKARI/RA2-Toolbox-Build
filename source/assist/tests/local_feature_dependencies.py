"""Run actual local selection, landing and player consumer transactions."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from startup_dependencies import block


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cxx', default='g++')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    here = Path(__file__).resolve().parent
    source = here.parent / 'source'
    text = (source / 'player_test_bridge.h').read_text()
    prefix = text[text.index('struct PlayerIdentity'):text.index('static bool playableHouse')]
    routes = out / 'player-routes.inc'
    routes.write_text(prefix + '\n' + '\n'.join(block(text, name) for name in (
        'static bool samePlayerWorld(', 'static void clearPlayerSidebar(',
        'static void restorePlayerContext(', 'static int switchPlayerContext(',
        'static void rebuildPlayerSidebar(')) + '\n')
    paths = [source / p for p in ('type_select_bridge.h', 'type_hotkey.h',
        'magnetron_drop_bridge.h', 'magnetron_drop.h', 'player_test_bridge.h')]
    paths += [here / 'local_feature_dependencies.cpp', Path(__file__).resolve()]
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    hashes = {str(p): digest(p) for p in paths}
    executable = out / 'local-feature-dependencies'
    command = [args.cxx, '-std=c++20', '-O2', '-UNDEBUG',
        '-DLOCAL_PLAYER_ROUTES="' + routes.as_posix() + '"',
        str(here / 'local_feature_dependencies.cpp'), '-o', str(executable)]
    build = subprocess.run(command, capture_output=True, text=True)
    (out / 'build.log').write_text(build.stdout + build.stderr)
    if build.returncode:
        raise RuntimeError(build.stdout + build.stderr)
    run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
    (out / 'run.log').write_text(run.stdout + run.stderr)
    if run.returncode:
        raise RuntimeError(run.stdout + run.stderr)
    assert hashes == {str(p): digest(p) for p in paths}
    result = {'schema': 'ra2.local-feature-dependencies.v1',
        'scope': __doc__, 'source_sha256': hashes,
        'player_switch_shipping_enabled': False,
        'player_switch_enabled_branch_is_synthetic': True,
        **json.loads(run.stdout)}
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(run.stdout, end='')


if __name__ == '__main__':
    main()
