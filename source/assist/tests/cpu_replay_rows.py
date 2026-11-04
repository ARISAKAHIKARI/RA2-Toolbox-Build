"""Run current row replay beside the saved pre-change production record."""
import argparse
import hashlib
from pathlib import Path
import subprocess
p = argparse.ArgumentParser()
p.add_argument('--zig', required=True)
p.add_argument('--baseline-record', required=True)
p.add_argument('--build-dir', required=True)
a = p.parse_args()
source = Path(__file__).resolve()
headers = source.parents[2] / 'external_reader' / 'source'
build = Path(a.build_dir).resolve()
build.mkdir(parents=True, exist_ok=True)
baseline = Path(a.baseline_record).read_bytes()
original = baseline.decode('utf-8-sig')
namespace = 'namespace external { namespace camera {'
assert original.count(namespace) == 1
reference = build / 'record_reference.h'
reference.write_text(original.replace(namespace, 'namespace external { namespace record_reference {\nusing camera::Surface;using camera::Light;using camera::Material;'), encoding='utf-8')
wrapper = build / 'fixture.cpp'
wrapper.write_text('#define RA2_REFERENCE_RECORD_HEADER "' + reference.as_posix() + '"\n#include "' + source.with_suffix('.cpp').as_posix() + '"\n', encoding='utf-8')
exe = build / 'cpu_replay_rows.exe'
cmd = [str(Path(a.zig).resolve()), 'c++', '-target', 'x86_64-windows-gnu', '-std=c++17', '-O2', str(wrapper), str(source.parents[2] / 'third_party' / 'xbrz' / 'xbrz.cpp'), '-I', str(headers), '-o', str(exe), '-lgdi32', '-luser32']
result = subprocess.run(cmd, capture_output=True, text=True)
(build / 'build.log').write_text(result.stdout + result.stderr, encoding='utf-8')
if result.returncode:
    print(result.stdout + result.stderr)
    raise SystemExit(result.returncode)
result = subprocess.run([str(exe)], capture_output=True, text=True)
report = 'baseline_sha256=' + hashlib.sha256(baseline).hexdigest() + '\n' + result.stdout + result.stderr
(build / 'test.log').write_text(report, encoding='utf-8')
print(report)
raise SystemExit(result.returncode)
