"""Actual Context -> actor/raster cache -> SceneRenderer recovery.

Reuses the established mock Win32 file and process boundary, with separate
dynamic-sample and live-resource failures. It does not emulate production
capture/decode/depth functions. Optional baseline-header mode compiles the
same assertions against a preserved pre-fix camera_assets.h.
"""
from pathlib import Path
import argparse
import ast
import hashlib
import json
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument("--output", type=Path)
parser.add_argument("--baseline-header", type=Path)
parser.add_argument("--expect-failure", action="store_true")
args = parser.parse_args()
out = (args.output or Path(tempfile.mkdtemp(prefix="building-depth-"))).resolve()
out.mkdir(parents=True, exist_ok=True)
boundary = root / "source/assist/tests/game_assets_host_test.py"
module = ast.parse(boundary.read_text())
values = {
    n.targets[0].id: ast.literal_eval(n.value)
    for n in module.body
    if isinstance(n, ast.Assign)
    and isinstance(n.targets[0], ast.Name)
    and n.targets[0].id in ("header", "body")
}
(out / "windows.h").write_text(values["header"])
prefix = values["body"].split("int main(int argc,char**argv){", 1)[0]
fault = r'''
static uint32_t faultAddress=0;static size_t faultBytes=0,faultHits=0;
static bool faultLive=false;static unsigned faultSkip=0;
static void resetFault(){faultAddress=0;faultBytes=faultHits=0;faultSkip=0;faultLive=false;}
static void setFault(uint32_t address,size_t bytes,bool live=false,unsigned skip=0){resetFault();faultAddress=address;faultBytes=bytes;faultLive=live;faultSkip=skip;}
static bool failRead(bool live,uint32_t p,size_t n){if(!faultBytes||live!=faultLive||uint64_t(p)+n<=faultAddress||uint64_t(faultAddress)+faultBytes<=p)return false;return ++faultHits>faultSkip;}
'''
prefix = prefix.replace("namespace external {struct ReadSession {", fault + "\nnamespace external {struct ReadSession {", 1)
old = "++rawReads;if(uint64_t(p)+n>memory.size())"
assert prefix.count(old) == 1
prefix = prefix.replace(old, "++rawReads;if(failRead(false,p,n))return false;if(uint64_t(p)+n>memory.size())", 1)
old = "};}\nstatic external::ReadSession"
assert prefix.count(old) == 1
prefix = prefix.replace(old, "bool readLive(uint32_t p,void*out,size_t n){if(failRead(true,p,n)||uint64_t(p)+n>memory.size())return false;std::memcpy(out,memory.data()+p,n);return true;}\n" + old, 1)
if args.baseline_header:
    original = '#include "source/assist/source/game_assets_host.h"'
    host = (root / "source/assist/source/game_assets_host.h").read_text().splitlines()
    assert prefix.count(original) == 1
    # Keep just this override beside its quoted include. The provenance folder
    # may also contain unchanged headers, which must retain one include owner.
    baseline_dir = out / "baseline-header"
    baseline_dir.mkdir(exist_ok=True)
    baseline_copy = baseline_dir / "camera_assets.h"
    baseline_copy.write_bytes(args.baseline_header.read_bytes())
    replacement = f'#include "{baseline_copy}"\n' + "\n".join(host[1:])
    prefix = prefix.replace(original, replacement, 1)
fixture = Path(__file__).with_suffix(".cpp")
generated = out / "production_fixture.cpp"
generated.write_text(prefix + fixture.read_text())
compile_cmd = ["g++", "-std=c++17", "-O0", "-pthread", "-I" + str(out), "-I" + str(root),
               "-I" + str(root / "source/external_reader/source"), "-MMD", "-MF", str(out / "production.d"),
               "-c", str(generated), "-o", str(out / "production.o")]
link_cmd = ["g++", "-std=c++17", "-O0", "-pthread", str(out / "production.o"),
            str(root / "source/third_party/xbrz/xbrz.cpp"), "-ldl", "-o", str(out / "fixture")]
for stage, cmd in (("compile", compile_cmd), ("link", link_cmd)):
    run = subprocess.run(cmd, text=True, capture_output=True, cwd=root)
    (out / (stage + ".log")).write_text(run.stdout + run.stderr)
    if run.returncode:
        sys.stderr.write(run.stdout + run.stderr)
        raise SystemExit(run.returncode)
deps = (out / "production.d").read_text().replace("\\\n", " ").split(":", 1)[1].split()
paths = {Path(p).resolve() for p in deps}
paths.update((Path(__file__).resolve(), fixture.resolve(), boundary, root / "source/third_party/xbrz/xbrz.cpp"))
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths) if p.is_file()}
run = subprocess.run([str(out / "fixture")], text=True, capture_output=True, cwd=root)
(out / "run.log").write_text(run.stdout + run.stderr)
receipt = {"test": "building_depth_capture", "boundary": "synthetic Win32/files/process only",
           "production": "Context, SHP/TMP decoding, SceneRenderer caches, native Surface/GPU record",
           "baseline_header": str(args.baseline_header.resolve()) if args.baseline_header else None,
           "exit_code": run.returncode, "expected_failure": args.expect_failure,
           "compile": compile_cmd, "link": link_cmd, "sha256": hashes,
           "stdout": run.stdout, "stderr": run.stderr}
(out / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
print(run.stdout, end="")
sys.stderr.write(run.stderr)
if args.expect_failure:
    raise SystemExit(0 if run.returncode == 1 and "FAIL " in run.stderr else 1)
raise SystemExit(run.returncode)
