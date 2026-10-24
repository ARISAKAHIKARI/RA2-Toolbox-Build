"""Verify the shipped update and package tracked source separately for release."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / 'packages/v1.0.38-pre.1'
DIST = ROOT / 'dist/v1.0.38-pre.1'
UPDATE = 'RA2-Toolbox-v1.0.38-pre.1-Update.zip'
SOURCE = 'RA2-Toolbox-v1.0.38-pre.1-Source.zip'
EXPECTED = 'b1f1758da1b7e66cf637715440c5296d96bf4b32bf1e666c633e80b40a5ea324'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def prepare():
    manifest = json.loads((PACKAGE / 'manifest.json').read_text())
    assert manifest['version'] == '1.0.38-pre.1'
    assert manifest['file_version'] == '1.0.38.1'
    assert len(manifest['assets']) == 1
    asset = manifest['assets'][0]
    update = (PACKAGE / UPDATE).read_bytes()
    assert asset['name'] == UPDATE
    assert len(update) == asset['bytes']
    assert digest(update) == asset['sha256'] == EXPECTED
    with zipfile.ZipFile(PACKAGE / UPDATE) as archive:
        assert archive.testzip() is None
        assert set(archive.namelist()) == {'RA2Toolbox.exe', 'GameRuntime.dll', 'README-Windows.txt', 'RELEASE_NOTES.md'}
        for name, info in asset['files'].items():
            data = archive.read(name)
            assert len(data) == info['bytes'] and digest(data) == info['sha256']
    for report in ('direct-runtime','controlled-tech','direct-tech'):
        proof=json.loads((ROOT / 'source/verification/review-138-pre1' / (report+'.json')).read_text())
        assert proof['dll_sha256']==asset['files']['GameRuntime.dll']['sha256']
        assert proof['results'] and all(row['status']=='PASS' for row in proof['results'])
    DIST.mkdir(parents=True, exist_ok=True)
    (DIST / UPDATE).write_bytes(update)
    paths = subprocess.check_output([
        'git', 'ls-files', '-z', '--', 'source', 'AGENTS.md', 'README.md',
        '.github/workflows/publish-v138-pre1.yml',
        'packages/v1.0.38-pre.1/README-Windows.txt',
        'packages/v1.0.38-pre.1/RELEASE_NOTES.md',
        'packages/v1.0.38-pre.1/manifest.json',
    ], cwd=ROOT).decode().strip('\0').split('\0')
    assert len(paths) > 600
    with zipfile.ZipFile(DIST / SOURCE, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name in sorted(paths):
            path = Path(name)
            assert not path.is_absolute() and '..' not in path.parts
            assert path.suffix.lower() not in {'.exe', '.dll', '.pdb', '.zip', '.7z', '.o', '.lib'}
            assert not (ROOT / path).is_symlink()
            info = zipfile.ZipInfo(name, (2026, 10, 25, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, (ROOT / path).read_bytes())
    with zipfile.ZipFile(DIST / SOURCE) as archive:
        assert archive.testzip() is None and set(archive.namelist()) == set(paths)
        for name in paths:
            assert archive.read(name) == (ROOT / name).read_bytes()
    source = (DIST / SOURCE).read_bytes()
    output = {
        'version': manifest['version'],
        'source_commit': os.environ.get('GITHUB_SHA') or subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'assets': [asset, {'name': SOURCE, 'bytes': len(source), 'sha256': digest(source), 'tracked_files': len(paths)}],
    }
    (DIST / 'release-manifest.json').write_text(json.dumps(output, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(output, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    prepare()
