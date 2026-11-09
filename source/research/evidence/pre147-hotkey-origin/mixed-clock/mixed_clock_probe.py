#!/usr/bin/env python3
"""Compile exact historical age predicates and hotkey-mode functions.

This fixture controls the two input clocks. It does not measure Windows clock
skew, execute the whole UI, or establish what occurred in the uploaded log.
"""
import hashlib
import json
import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parent
HISTORY = ROOT.parent / "registry-history"
OUT = ROOT / "mixed-clock-evidence"
OUT.mkdir(exist_ok=True)
COMMITS = {
    "v143": "bf3fbe88faa96513649f2998aa0ab6e71935f4a8",
    "v1464": "f452d1cb95c57f465356b55a3d7b83fb7403a70a",
}
INTRODUCING = "944586abf9cd331d2aaf9fe24af744a593bd31d3"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def extract(text, pattern):
    found = list(re.finditer(pattern, text, re.S))
    assert len(found) == 1, (pattern, len(found))
    hit = found[0]
    return {
        "start_line": text.count("\n", 0, hit.start()) + 1,
        "end_line": text.count("\n", 0, hit.end()) + 1,
        "text": hit.group(0),
        "extracted_sha256": hashlib.sha256(hit.group(0).encode()).hexdigest(),
    }


sources = {}
fragments = {}
for version in COMMITS:
    path = HISTORY / version / "main.cpp"
    text = path.read_text()
    sources[version] = {"path": str(path), "commit": COMMITS[version], "sha256": sha(path)}
    fragments[version] = {
        "read_status": extract(text, r"snap\.readStatus=d\.status==1&&d\.sampledTick&&DWORD\(displayNow-d\.sampledTick\)>=2000[^;]*;"),
        "valid_and_render": extract(text, r"const bool externalValid=d\.status==1&&d\.sampledTick&&.*?snap\.renderStatus=externalValid&&d\.renderStatus==1\?1:0;"),
        "desired_hotkey_mode": extract(text, r"static int desiredHotkeyMode\(\)\{.*?\n\}"),
    }

assert fragments["v143"]["desired_hotkey_mode"]["text"] == fragments["v1464"]["desired_hotkey_mode"]["text"]
for filename in ("clock_ms.h", "world_frame_runtime.h"):
    path = ROOT / "v1464" / filename
    sources[filename] = {"path": str(path), "commit": COMMITS["v1464"], "sha256": sha(path)}
patch = ROOT / "944586-main.patch"
sources["introducing_patch"] = {"path": str(patch), "commit": INTRODUCING, "sha256": sha(patch)}
context_lines = {
    "v143/main.cpp": {"old_sample_uses_display_clock": [1252, 1252]},
    "v1464/main.cpp": {
        "merge_uses_host_display_clock": [1242, 1251],
        "confirmation_reset_before_attempt": [1288, 1288],
        "missing_token_returns": [1292, 1296],
        "same_frame_confirmation": [1308, 1310],
        "producer_tick_and_incomplete_return": [1311, 1314],
        "read_snapshot_calls_merge": [1462, 1462],
        "hotkey_mode_change_reregisters": [2272, 2272],
    },
    "v1464/clock_ms.h": {"separate_gettick_and_qpc_clock_domains": [8, 25]},
    "v1464/world_frame_runtime.h": {"producer_key_tick_uses_GetTickCount": [33, 34]},
}

cpp = [r'''#include <cstdint>
#include <cstdio>
#include <cstdlib>
using DWORD=uint32_t;
struct Display {int status=1;DWORD sampledTick=0;int renderStatus=1;};
struct State {int readStatus=0,renderStatus=0,status=1,chat=0;};
struct Result {int readStatus,renderStatus,mode;};
''']
for version, part in fragments.items():
    cpp.append("namespace " + version + " {\n")
    cpp.append("static State snap;static bool enabled=true;struct Extra {bool watchSecondary=false;} extra;static bool foregroundGame(){return true;}\n")
    cpp.append(part["desired_hotkey_mode"]["text"] + "\n")
    cpp.append("Result probe(const Display&d,DWORD displayNow,bool externalSourceConfirmed){snap={};\n")
    cpp.append(part["read_status"]["text"] + "\n")
    cpp.append(part["valid_and_render"]["text"] + "\n")
    cpp.append("return {snap.readStatus,snap.renderStatus,desiredHotkeyMode()};}\n}\n")

cases = [
    ("old_host_clock_same_time", "v143", 1000, 1000, False, 1, 1, (1, 1, 2)),
    ("new_producer_ahead_8ms_unconfirmed", "v1464", 1008, 1000, False, 1, 1, (0, 0, 0)),
    ("new_producer_ahead_8ms_confirmed", "v1464", 1008, 1000, True, 1, 1, (1, 1, 2)),
    ("new_producer_ahead_1ms_unconfirmed", "v1464", 1008, 1007, False, 1, 1, (0, 0, 0)),
    ("new_clock_catches_up", "v1464", 1008, 1008, False, 1, 1, (1, 1, 2)),
    ("new_fresh_past_sample", "v1464", 1008, 1024, False, 1, 1, (1, 1, 2)),
    ("new_true_expiry_2000ms", "v1464", 1000, 3000, False, 1, 1, (0, 0, 0)),
    ("new_independently_confirmed_old_frame", "v1464", 1000, 3000, True, 1, 1, (1, 1, 2)),
    ("new_confirmed_invalid_display_sample", "v1464", 1000, 1008, True, 0, 1, (0, 0, 0)),
    ("new_confirmed_hidden_display", "v1464", 1000, 1008, True, 1, 0, (1, 0, 0)),
    ("new_uint32_wrap_fresh", "v1464", 0xfffffff0, 0x10, False, 1, 1, (1, 1, 2)),
]
cpp.append("int main(){\n")
for name, version, tick, now, confirmed, status, render, expected in cases:
    cpp.append("{const Display d{" + f"{status},{tick}u,{render}" + "};const DWORD now=" + str(now) + "u;const auto r=" + version + "::probe(d,now," + str(confirmed).lower() + ");\n")
    cpp.append(f"if(r.readStatus!={expected[0]}||r.renderStatus!={expected[1]}||r.mode!={expected[2]}){{std::fprintf(stderr,\"FAIL {name}\\n\");return 1;}}\n")
    cpp.append("std::printf(\"{\\\"case\\\":\\\"" + name + "\\\",\\\"unsigned_age\\\":%u,\\\"read_status\\\":%d,\\\"render_status\\\":%d,\\\"desired_hotkey_mode\\\":%d}\\n\",unsigned(DWORD(now-d.sampledTick)),r.readStatus,r.renderStatus,r.mode);}\n")
cpp.append("return 0;}\n")
source = OUT / "probe.cpp"
source.write_text("".join(cpp))
binary = OUT / "probe"
compile_command = ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", str(source), "-o", str(binary)]
compiled = subprocess.run(compile_command, text=True, capture_output=True)
(OUT / "compile.log").write_text(compiled.stdout + compiled.stderr)
if compiled.returncode:
    raise SystemExit(compiled.returncode)
ran = subprocess.run([str(binary)], text=True, capture_output=True)
(OUT / "run.log").write_text(ran.stdout + ran.stderr)
rows = [json.loads(line) for line in ran.stdout.splitlines()]
assert ran.returncode == 0 and len(rows) == len(cases)
report = {
    "passed": True,
    "scope": "compiled exact historical mergeExternal age/read/render statements and desiredHotkeyMode functions; synthetic clock inputs, not the complete UI or native Windows/game",
    "introducing_commit": INTRODUCING,
    "case_count": len(cases), "checks": len(cases) * 3,
    "assumptions": [
        "Healthy command side is represented by snap.status=1; foreground=true, enabled=true and chat=false.",
        "The 8ms future producer timestamp is a deliberately chosen model input, not a measured Windows clock skew or an observed timestamp from the user log.",
        "confirmed=false models an incomplete sampling attempt followed by merge; this fixture does not execute WorldFrameScope, readSnapshot, registration APIs or key arrival timing.",
        "confirmed=true bypasses the age check exactly as production does; independent producer confirmation itself is not simulated or inferred from wall-clock age.",
        "The modeled future-tick rejection stops when displayNow reaches sampledTick. Repeated short context loss is possible but a persistent outage is not established.",
    ],
    "sources": sources, "fragments": fragments, "context_source_lines": context_lines,
    "commands": {"compile": compile_command, "run": [str(binary)]},
    "exit_codes": {"compile": compiled.returncode, "run": ran.returncode},
    "results": rows,
    "artifacts": {p.name: sha(p) for p in (pathlib.Path(__file__), source, binary, OUT / "compile.log", OUT / "run.log")},
}
(OUT / "result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
print(json.dumps({"passed": True, "cases": len(cases), "checks": len(cases) * 3, "result": str(OUT / "result.json")}, ensure_ascii=False))
