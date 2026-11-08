"""Exercise the actual Assist whole-section save entry point and INI Store.

The native writer is controlled: one section write fails while ordinary writes
succeed. This isolates whether the production dispatch owns the failed intent,
keeps the close gate shut, and can replay every MULTI_SZ entry on explicit retry.
No Windows filesystem or live game is used by this portable regression.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

FILES = ("shared/source/async_ini_store.h", "assist/source/main.cpp")

FIXTURE = r'''
#include "store.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
using namespace std::chrono_literals;
using asyncini::core::Operation;
using asyncini::core::Result;
using asyncini::core::Kind;
static const int FALSE=0;
static const auto owner=std::this_thread::get_id();
static std::atomic<bool> failSection{true};
static std::atomic<unsigned> nativeCalls{0},ownerCalls{0};
static std::wstring disk;
static int nativeSection(const wchar_t*,const wchar_t* block,const wchar_t*){
 ++nativeCalls;if(std::this_thread::get_id()==owner)++ownerCalls;
 if(failSection.load())return FALSE;
 const wchar_t*end=block;while(*end)end+=std::char_traits<wchar_t>::length(end)+1;
 disk.assign(block,end+1);return 1;
}
static asyncini::core::Store store([](const Operation&o){
#if SECTION_QUEUE_ENABLED
 if(o.kind==Kind::ReplaceSection)return Result{bool(nativeSection(o.section.pointer(),o.value.pointer(),o.path.pointer())),32};
#endif
 return Result{};
});
static std::wstring wide(const char*text){std::string s=text;return std::wstring(s.begin(),s.end());}
static std::wstring path(const wchar_t*name){return name;}
namespace typecatalog {static bool validId(const std::string&s){return !s.empty();}}
#if SECTION_QUEUE_ENABLED
#define WritePrivateProfileSectionW(s,b,p) store.writeSection(s,b,p)
#else
#define WritePrivateProfileSectionW nativeSection
#endif
#include "write-section.inc"
#undef WritePrivateProfileSectionW
template<class F>void eventually(F f){
 const auto end=std::chrono::steady_clock::now()+5s;
 while(!f()){assert(std::chrono::steady_clock::now()<end);std::this_thread::yield();}
}
int main(){
 const bool admitted=writeSection(L"TypeHotkeys",{{"E1",65},{"E2",66}});
 assert(store.write(L"Display",L"FPS",L"144",L"Assist.ini"));
 eventually([]{return store.idle();});
 const auto before=store.status();
 const bool closeGateOpen=store.idle()&&!before.failed;
 failSection=false;const bool retried=store.retryFailed();eventually([]{return store.idle();});
 const bool allEntries=disk==std::wstring(L"E1=65\0E2=66\0\0",13);
 std::cout<<"{\"section_api_calls\":"<<nativeCalls
  <<",\"section_api_calls_on_owner\":"<<ownerCalls
  <<",\"admitted\":"<<(admitted?"true":"false")
  <<",\"failed_intent_visible\":"<<(before.failed?"true":"false")
  <<",\"close_gate_open_after_failure\":"<<(closeGateOpen?"true":"false")
  <<",\"explicit_retry_admitted\":"<<(retried?"true":"false")
  <<",\"complete_section_persisted_after_retry\":"<<(allEntries?"true":"false")
  <<",\"error_cleared_after_retry\":"<<(!store.status().failed?"true":"false")
  <<",\"windows_executed\":false}"<<std::endl;
}
'''


def run(label, sources, output, compiler):
    folder = output / label
    folder.mkdir(parents=True, exist_ok=True)
    main = sources["assist/source/main.cpp"].decode().replace("\r\n", "\n")
    match = re.search(r"static bool writeSection\([^\n]+\)\{.*?\n\}", main, re.S)
    assert match, "Production whole-section entry point not found"
    enabled = bool(re.search(r"^#define WritePrivateProfileSectionW asyncini::writeSection$", main, re.M))
    (folder / "store.h").write_bytes(sources["shared/source/async_ini_store.h"])
    (folder / "write-section.inc").write_text(match.group(0) + "\n")
    (folder / "test.cpp").write_text(FIXTURE)
    build = subprocess.run([compiler, "-std=c++17", "-O2", "-pthread",
                            "-DSECTION_QUEUE_ENABLED=" + str(int(enabled)),
                            str(folder / "test.cpp"), "-o", str(folder / "test")],
                           capture_output=True, text=True, timeout=60)
    (folder / "build.log").write_text(build.stdout + build.stderr)
    assert build.returncode == 0, str(folder / "build.log")
    executed = subprocess.run([str(folder / "test")], capture_output=True, text=True, timeout=15)
    (folder / "run.log").write_text(executed.stdout + executed.stderr)
    assert executed.returncode == 0, str(folder / "run.log")
    result = json.loads(executed.stdout)
    result.update(label=label, section_alias_enabled=enabled,
                  source_sha256={name: hashlib.sha256(data).hexdigest() for name, data in sources.items()})
    if enabled:
        assert result["admitted"] and result["failed_intent_visible"]
        assert not result["close_gate_open_after_failure"]
        assert result["complete_section_persisted_after_retry"] and result["error_cleared_after_retry"]
        assert result["section_api_calls_on_owner"] == 0 and result["section_api_calls"] == 2
    else:
        assert not result["admitted"] and not result["failed_intent_visible"]
        assert result["close_gate_open_after_failure"] and not result["complete_section_persisted_after_retry"]
        assert result["section_api_calls_on_owner"] == 1 and result["section_api_calls"] == 1
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--out", type=Path)
    parser.add_argument("--baseline-revision")
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    output = args.out.resolve() if args.out else Path(tempfile.mkdtemp(prefix="assist-section-save-"))
    output.mkdir(parents=True, exist_ok=True)
    sources = {name: (args.source / name).read_bytes() for name in FILES}
    runs = [run("current", sources, output, args.compiler)]
    assert runs[0]["section_alias_enabled"], "Assist still bypasses the asynchronous section writer"
    if args.baseline_revision:
        old = {name: subprocess.check_output(["git", "-C", str(args.source.parent), "show",
                                               args.baseline_revision + ":source/" + name]) for name in FILES}
        runs.append(run("baseline", old, output, args.compiler))
    assert all((args.source / name).read_bytes() == data for name, data in sources.items()), "Source changed during test"
    receipt = {"passed": True, "schema": "ra2.assist-section-save.regression.v1", "runs": runs,
               "baseline_revision": args.baseline_revision, "windows_executed": False,
               "scope": "Production writeSection function, dispatch alias and Store; controlled native failure and close storage predicate. Full owner WM_CLOSE flow, native Win32 I/O, ACL/file locks and live game were not executed."}
    (output / "receipt.json").write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(receipt, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
