"""Execute actual entry client/manager/Policy with modeled Win32 and patch boundaries."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SHARED = ROOT / "source/shared/source"


def function(text, signature):
    start = text.index(signature)
    brace = text.index("{", start)
    end, depth = brace + 1, 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


WINDOWS = r'''#pragma once
#include <atomic>
#include <cstdint>
#define __stdcall
#define __cdecl
#define __declspec(x)
using LONG=int32_t;using DWORD=uint32_t;using HANDLE=void*;using HMODULE=void*;using LPCWSTR=const wchar_t*;
constexpr unsigned GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS=1,GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT=2;
inline std::atomic<uint64_t>fixtureLocked{0};inline thread_local uint32_t fixtureThread=1;
inline LONG InterlockedCompareExchange(volatile LONG*p,LONG value,LONG compare){++fixtureLocked;return __sync_val_compare_and_swap(p,compare,value);}
inline LONG InterlockedExchange(volatile LONG*p,LONG value){++fixtureLocked;return __sync_lock_test_and_set(p,value);}
inline LONG InterlockedIncrement(volatile LONG*p){++fixtureLocked;return __sync_add_and_fetch(p,1);}
inline DWORD GetCurrentThreadId(){return fixtureThread;}
inline bool GetModuleHandleExW(unsigned,LPCWSTR,HMODULE*out){*out=reinterpret_cast<void*>(uintptr_t(0x20000000));return true;}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", default="061e106b71c81d79ea687edf36ad8401ffb722f8")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    names = ["game_entry_api.h", "game_entry_client.h", "entry_policy.h", "runtime_seh_scope.h"]
    before = {name: subprocess.check_output(["git", "show", args.baseline + ":source/shared/source/" + name], cwd=ROOT).decode() for name in names}
    before["game_entry_manager.cpp"] = subprocess.check_output(["git", "show", args.baseline + ":source/shared/source/game_entry_manager.cpp"], cwd=ROOT).decode()
    after = {name: (SHARED / name).read_text() for name in names + ["game_entry_validation.h", "game_entry_manager.cpp"]}
    records = {}
    for label, files in [("before", before), ("after", after)]:
        directory = out / label; directory.mkdir(exist_ok=True)
        for name, content in files.items():
            (directory / name).write_text(content)
        (directory / "windows.h").write_text(WINDOWS)
        manager = files["game_entry_manager.cpp"]
        globals_ = manager[manager.index("static Policy policy;"):manager.index("static assist::hookregistry")]
        (directory / "manager_globals.inc").write_text(globals_)
        body = function(manager, 'extern "C" __declspec(dllexport) int32_t __stdcall ManageGameEntry(')
        body = body.replace("{", "{++managerCalls;", 1)
        if label == "after":
            body += "\n" + function(manager, "bool gameentry::validateCurrent(").replace("{", "{++validationCalls;", 1)
        (directory / "manager_body.inc").write_text(body)
        exe = directory / "entry-hotpath"
        command = ["g++", "-std=c++17", "-O2", "-pthread", "-UNDEBUG", "-fsanitize=undefined", "-fno-sanitize-recover=all", "-I" + str(directory), str(HERE / "entry_hotpath.cpp"), "-o", str(exe)]
        if label == "after":
            command.insert(1, "-DHOT_VALIDATION=1")
        build = subprocess.run(command, capture_output=True, text=True)
        (directory / "build.log").write_text(build.stdout + build.stderr)
        if build.returncode:
            raise RuntimeError(build.stdout + build.stderr)
        run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
        (directory / "run.log").write_text(run.stdout + run.stderr)
        if run.returncode:
            raise RuntimeError(run.stdout + run.stderr)
        records[label] = json.loads(run.stdout)
    assert records["before"]["semantic_digest"] == records["after"]["semantic_digest"]
    paths = [SHARED / name for name in after] + [HERE / "entry_hotpath.cpp", Path(__file__)]
    report = {"pass": True, "baseline_commit": args.baseline, "scope": __doc__, "results": records,
              "source_sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
              "instrumentation": "One call-counter increment at each extracted manager/validation entry; Win32 interlocked calls counted at the modeled OS boundary.",
              "windows_executed": False, "game_tested": False}
    (out / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(records, indent=2))


if __name__ == "__main__":
    main()
