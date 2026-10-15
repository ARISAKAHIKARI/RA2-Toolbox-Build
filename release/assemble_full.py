#!/usr/bin/env python3
"""Reconstruct the exact validated Full ZIP and prepare its release assets."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--manifest', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    meta = json.loads((root / args.manifest).read_text('utf-8'))
    output = (root / args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    target = output / meta['asset_name']
    temporary = target.with_suffix('.partial')
    full_hash = hashlib.sha256()
    total = 0
    with temporary.open('wb') as stream:
        for record in meta['parts']:
            path = (root / record['path']).resolve()
            if root not in path.parents:
                raise ValueError('part outside repository')
            block = path.read_bytes()
            if len(block) != record['bytes'] or hashlib.sha256(block).hexdigest() != record['sha256']:
                raise ValueError('invalid release part: ' + record['path'])
            stream.write(block)
            full_hash.update(block)
            total += len(block)
    if total != meta['bytes'] or full_hash.hexdigest() != meta['sha256']:
        raise ValueError('Full ZIP size/SHA256 mismatch')
    temporary.replace(target)
    with zipfile.ZipFile(target) as archive:
        if (bad := archive.testzip()) is not None:
            raise ValueError('ZIP CRC mismatch: ' + bad)
    lines = [meta['sha256'] + '  ' + target.name]
    for record in meta['additional_assets']:
        path = root / record['path']
        if path.stat().st_size != record['bytes'] or digest(path) != record['sha256']:
            raise ValueError('invalid additional asset: ' + record['path'])
        shutil.copy2(path, output / record['name'])
        lines.append(record['sha256'] + '  ' + record['name'])
    (output / 'SHA256SUMS.txt').write_text('\n'.join(lines) + '\n', encoding='ascii')
    print('PASS: exact Full ZIP, CRC, and all release asset checksums')
    print(json.dumps({'file': target.name, 'bytes': total, 'sha256': full_hash.hexdigest()}))


if __name__ == '__main__':
    main()
