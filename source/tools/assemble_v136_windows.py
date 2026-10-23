#!/usr/bin/env python3
"""Assemble the complete v1.0.36 runtime from pinned, published inputs."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / 'packages/v1.0.36'
BASE_NAME = 'RA2-Toolbox-v1.0.2-Windows.zip'
BASE_SHA = 'e5479b5e1b570e6412afd720059640089bd6912c3d02ae6f88719814588ac7bd'
OUTPUT_NAME = 'RA2-Toolbox-v1.0.36-Windows.zip'
FOLDERS = {'assets', 'profiles', 'engine', 'runtime', 'licenses'}
BINARIES = {'RA2Toolbox.exe', 'GameRuntime.dll'}
ENGINE = ('dis_render.py', 'player-template.html', 'precompute.py', 'preview_builder.py')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def members(archive):
    seen = set()
    names = []
    for info in archive.infolist():
        name = info.filename
        path = PurePosixPath(name)
        assert not path.is_absolute() and '..' not in path.parts, name
        assert '\\' not in name and ':' not in name, name
        assert (info.external_attr >> 16) & 0o170000 != 0o120000, name
        if info.is_dir():
            continue
        assert name.casefold() not in seen, 'duplicate member: ' + name
        seen.add(name.casefold())
        names.append(name)
    return sorted(names)


def assemble(baseline, output_dir):
    assert baseline.stat().st_size == 144179588
    assert sha(baseline.read_bytes()) == BASE_SHA, 'resource baseline mismatch'
    manifest = json.loads((PACKAGE / 'manifest.json').read_text())
    assert manifest['version'] == '1.0.36'
    assets = {a['name']: a for a in manifest['assets']}
    assert set(assets) == {'RA2-Toolbox-v1.0.36-Update.zip', 'RA2-Toolbox-v1.0.36-Source.zip'}
    for name, record in assets.items():
        data = (PACKAGE / name).read_bytes()
        assert len(data) == record['bytes'] and sha(data) == record['sha256'], name

    update = PACKAGE / 'RA2-Toolbox-v1.0.36-Update.zip'
    source = PACKAGE / 'RA2-Toolbox-v1.0.36-Source.zip'
    update_record = assets[update.name]
    expected = {}
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / OUTPUT_NAME
    with zipfile.ZipFile(baseline) as old, zipfile.ZipFile(update) as new, \
            zipfile.ZipFile(source) as src, \
            zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as dst:
        assert set(members(new)) == BINARIES
        assert new.testzip() is None and src.testzip() is None

        def write(name, data):
            assert name not in expected, name
            info = zipfile.ZipInfo(name, (2026, 10, 23, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            dst.writestr(info, data)
            expected[name] = sha(data)

        for name in members(old):
            if name in BINARIES | {'README.txt'} or name in {'engine/' + n for n in ENGINE}:
                continue
            assert name.split('/')[0] in FOLDERS or name == 'Start-Message.cmd', name
            write(name, old.read(name))
        for name in sorted(BINARIES):
            data = new.read(name)
            assert sha(data) == update_record['files'][name], name
            write(name, data)
        for name in ENGINE:
            write('engine/' + name, src.read('source/engine/' + name))
        write('README.txt', (PACKAGE / 'README-Windows.txt').read_bytes())

    required = {
        'runtime/python.exe', 'runtime/python312.dll', 'runtime/python312.zip',
        'runtime/python312._pth', 'runtime/Lib/site-packages/cv2/cv2.pyd',
        'assets/original.sha', 'assets/palette.bgra', 'assets/groups.json',
        'assets/pairs.txt', 'engine/precompute.py', 'engine/dis_render.py',
        'RA2Toolbox.exe', 'GameRuntime.dll', 'README.txt',
    }
    assert required <= expected.keys(), required - expected.keys()
    profiles = sorted(n for n in expected if n.startswith('profiles/') and n.endswith('/profile.ini'))
    assert len(profiles) == 3, profiles
    for profile in profiles:
        folder = profile.rsplit('/', 1)[0]
        for name in ('original.sha', 'palette.bgra', 'groups.json', 'pairs.txt'):
            assert folder + '/' + name in expected, (folder, name)
    assert any(n.startswith('licenses/') for n in expected)
    assert any(n.startswith('assets/timelines/') for n in expected)
    assert not any(n.lower().endswith(('gamemd.exe', 'gamemd-spawn.exe', '.pdb', '.dmp')) for n in expected)
    with zipfile.ZipFile(output) as archive:
        assert set(members(archive)) == expected.keys()
        assert archive.testzip() is None
        for name, digest in expected.items():
            assert sha(archive.read(name)) == digest, name
    report = {
        'version': '1.0.36', 'name': OUTPUT_NAME, 'bytes': output.stat().st_size,
        'sha256': sha(output.read_bytes()), 'files': len(expected),
        'resource_baseline': {'name': BASE_NAME, 'sha256': BASE_SHA},
        'published_inputs': manifest['assets'],
        'profiles': profiles,
        'engine_files': {n: h for n, h in expected.items() if n.startswith('engine/')},
        'checks': ['input hashes', 'safe archive paths', 'required resources',
                   'three complete profiles', 'ZIP CRC', 'every output file hash'],
    }
    (output_dir / 'full-manifest.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'dist')
    args = parser.parse_args()
    assemble(args.baseline, args.output_dir)
