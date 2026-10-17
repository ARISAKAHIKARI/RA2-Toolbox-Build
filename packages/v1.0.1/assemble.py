"""Assemble only the allowlisted runtime distribution; fail closed on any mismatch."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import sys
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sha = lambda b: hashlib.sha256(b).hexdigest()
meta = json.loads((HERE / 'manifest.json').read_text())
out = ROOT / 'dist'
out.mkdir(exist_ok=True)
base_file = out / 'base.tmp'
base_meta = json.loads((ROOT / meta['base_manifest']).read_text())
with base_file.open('wb') as f:
    for record in base_meta['parts']:
        b = (ROOT / record['path']).read_bytes()
        assert len(b) == record['bytes'] and sha(b) == record['sha256'], record['path']
        f.write(b)
assert base_file.stat().st_size == base_meta['bytes']
assert sha(base_file.read_bytes()) == base_meta['sha256']
assert sha((HERE / 'overlay.zip').read_bytes()) == meta['overlay_sha256']
assert sha((HERE / 'development.zip').read_bytes()) == meta['development_sha256']
# Verify expanded source/docs against the separately archived delivery.
with zipfile.ZipFile(HERE / 'development.zip') as dev:
    for name in dev.namelist():
        if name.startswith(('source/', 'docs/', 'verification/')):
            assert (ROOT / name).read_bytes() == dev.read(name), name
print('PASS: repository source, documentation and evidence match Development archive')
target = out / meta['asset_name']
with zipfile.ZipFile(base_file) as base, zipfile.ZipFile(HERE / 'overlay.zip') as overlay, zipfile.ZipFile(target, 'w') as dst:
    assert set(overlay.namelist()) == {n for n,r in meta['files'].items() if r['source']=='overlay'}
    for name, record in sorted(meta['files'].items()):
        p = PurePosixPath(name)
        assert not p.is_absolute() and '..' not in p.parts
        assert p.parts[0] not in {'source','verification','docs','third-party'}
        assert record['source'] in {'base','overlay'}
        b = overlay.read(name) if record['source']=='overlay' else base.read(meta['base_prefix']+name)
        assert len(b)==record['bytes'] and sha(b)==record['sha256'], name
        info = zipfile.ZipInfo(name, tuple(record["date_time"]))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = record["external_attr"]
        info.create_system = record["create_system"]
        dst.writestr(info,b,compresslevel=6)
base_file.unlink()
assert target.stat().st_size == meta['bytes'], 'archive size mismatch'
assert sha(target.read_bytes()) == meta['sha256'], 'archive hash mismatch'
with zipfile.ZipFile(target) as z:
    assert z.testzip() is None
    assert set(z.namelist()) == set(meta['files'])
print(json.dumps({'asset':target.name,'bytes':meta['bytes'],'sha256':meta['sha256'],'files':len(meta['files'])}))
