"""Actual Context -> saved scene layers -> Surface/SceneRenderer recovery.

Only Win32, files and process bytes are synthetic. The optional baseline
header runs identical assertions against the preserved production header.
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
out = (args.output or Path(tempfile.mkdtemp(prefix="scene-extras-"))).resolve()
out.mkdir(parents=True, exist_ok=True)
boundary = root / "source/assist/tests/game_assets_host_test.py"
module = ast.parse(boundary.read_text())
values = {n.targets[0].id: ast.literal_eval(n.value) for n in module.body
          if isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name)
          and n.targets[0].id in ("header", "body")}
(out / "windows.h").write_text(values["header"])
prefix = values["body"].split("int main(int argc,char**argv){", 1)[0]
fault = r'''
static uint32_t faultAddress=0;static size_t faultBytes=0,faultHits=0;
static void resetFault(){faultAddress=0;faultBytes=faultHits=0;}
static void setFault(uint32_t address,size_t bytes){resetFault();faultAddress=address;faultBytes=bytes;}
static bool failRead(uint32_t p,size_t n){if(!faultBytes||uint64_t(p)+n<=faultAddress||uint64_t(faultAddress)+faultBytes<=p)return false;++faultHits;return true;}
'''
prefix = prefix.replace("namespace external {struct ReadSession {", fault + "\nnamespace external {struct ReadSession {", 1)
old = "++rawReads;if(uint64_t(p)+n>memory.size())"
assert prefix.count(old) == 1
prefix = prefix.replace(old, "++rawReads;if(failRead(p,n))return false;if(uint64_t(p)+n>memory.size())", 1)
# Real include owners resolve one selected Layers header. No source body is
# rewritten, and the other copied headers are exact production bytes.
headers = out / "headers"
headers.mkdir(exist_ok=True)
header_sources = {}
for name in ("radar_scene.h", "world_scene_pipeline.h", "radar_decoration.h", "scene_extra_layers.h"):
    source = root / "source/external_reader/source" / name
    if name == "scene_extra_layers.h" and args.baseline_header:
        source = args.baseline_header.resolve()
    (headers / name).write_bytes(source.read_bytes())
    header_sources[str(headers / name)] = str(source)
fixture = Path(__file__).with_suffix(".cpp")
generated = out / "production_fixture.cpp"
generated.write_text(prefix + fixture.read_text())
compile_cmd = ["g++", "-std=c++17", "-O0", "-pthread", "-I" + str(headers), "-I" + str(out),
               "-I" + str(root), "-I" + str(root / "source/external_reader/source"),
               "-MMD", "-MF", str(out / "production.d"), "-c", str(generated), "-o", str(out / "production.o")]
link_cmd = ["g++", "-std=c++17", "-O0", "-pthread", str(out / "production.o"),
            str(root / "source/third_party/xbrz/xbrz.cpp"), "-ldl", "-o", str(out / "fixture")]
commands = []
for stage, cmd in (("compile", compile_cmd), ("link", link_cmd)):
    run = subprocess.run(cmd, text=True, capture_output=True, cwd=root)
    (out / (stage + ".log")).write_text(run.stdout + run.stderr)
    commands.append({"stage": stage, "command": cmd, "exit_code": run.returncode})
    if run.returncode:
        (out / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        sys.stderr.write(run.stdout + run.stderr)
        raise SystemExit(run.returncode)
deps = (out / "production.d").read_text().replace("\\\n", " ").split(":", 1)[1].split()
paths = {Path(p).resolve() for p in deps}
paths.update((Path(__file__).resolve(), fixture.resolve(), boundary, root / "source/third_party/xbrz/xbrz.cpp"))
paths.update(Path(p) for p in header_sources.values())
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths) if p.is_file()}
run_cmd = [str(out / "fixture")]
run = subprocess.run(run_cmd, text=True, capture_output=True, cwd=root)
(out / "run.log").write_text(run.stdout + run.stderr)
commands.append({"stage": "run", "command": run_cmd, "exit_code": run.returncode})
receipt = {"test": "scene_extra_recovery", "boundary": "synthetic Win32/files/process only",
           "production": "Context, SHP decoding, Layers, SceneRenderer and CPU Surface",
           "baseline_header": str(args.baseline_header.resolve()) if args.baseline_header else None,
           "header_sources": header_sources, "exit_code": run.returncode, "expected_failure": args.expect_failure,
           "commands": commands, "sha256": hashes, "stdout": run.stdout, "stderr": run.stderr}
(out / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
print(run.stdout, end="")
sys.stderr.write(run.stderr)
if args.expect_failure:
    raise SystemExit(0 if run.returncode == 1 and "FAIL " in run.stderr else 1)
raise SystemExit(run.returncode)
