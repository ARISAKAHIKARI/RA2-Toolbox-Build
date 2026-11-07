"""Exercise the production INI store with a conservative Assist save trace.

Only calls whose section and key are literal strings are replayed, in source
order. Dynamic-key loops, native section writes and catalogue writes are omitted;
this is a queue/backpressure regression, not execution of Win32 profile I/O or
the complete Assist save function. A historical baseline may be run explicitly.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
import tempfile


BASELINE = "76b28dc4bb3ca2fb2e1a8769fefe6ba548554e17"
SOURCE_FILES = ("shared/source/async_ini_store.h", "assist/source/main.cpp",
                "assist/source/extra_settings_io.h")
CALL = re.compile(r'\b(write|WritePrivateProfileStringW)\s*\(\s*L"([^"\\]*)"\s*,\s*L"([^"\\]*)"\s*,\s*([^,;\n]+)')

FIXTURE = r'''
#include "store.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <set>
using namespace std::chrono_literals;
using asyncini::core::Operation;
using asyncini::core::Result;
using asyncini::core::Kind;
struct Gate {
 std::mutex mutex;std::condition_variable cv;bool entered=false,released=false;
 void wait(){std::unique_lock<std::mutex>l(mutex);entered=true;cv.notify_all();assert(cv.wait_for(l,20s,[&]{return released;}));}
 void await(){std::unique_lock<std::mutex>l(mutex);assert(cv.wait_for(l,5s,[&]{return entered;}));}
 void release(){std::lock_guard<std::mutex>l(mutex);released=true;cv.notify_all();}
};
template<class F>void eventually(F f){
 const auto end=std::chrono::steady_clock::now()+5s;
 while(!f()){assert(std::chrono::steady_clock::now()<end);std::this_thread::yield();}
}
struct Item {const wchar_t*section;const wchar_t*key;bool deleteKey;};
static const Item trace[]={
#include "trace.inc"
};
static std::map<std::wstring,int>profile;
static bool manualReveal(){
 struct {bool manualReveal=false;}extra;
 auto value=[](const wchar_t*section,const wchar_t*key,int fallback){
  assert(std::wstring(section)==L"Experimental");auto found=profile.find(key);
  return found==profile.end()?fallback:found->second;
 };
#include "manual-reveal.inc"
 return extra.manualReveal;
}
int main(){
 assert(!manualReveal());profile[L"RevealMap"]=1;assert(manualReveal());
 profile[L"RevealMap"]=0;assert(!manualReveal());
 profile[L"RevealMap"]=1;profile[L"ManualReveal"]=0;assert(!manualReveal());
 profile[L"RevealMap"]=0;profile[L"ManualReveal"]=1;assert(manualReveal());
 Gate gate;std::atomic<unsigned>calls{0};
 std::map<asyncini::core::Key,std::wstring>disk;
 const auto legacyKey=asyncini::core::keyOf(Operation(L"Experimental",L"RevealMap",L"1",L"Assist.ini"));
 disk[legacyKey]=L"1";
 asyncini::core::Store store([&](const Operation&o){
  if(calls++==0)gate.wait();const auto key=asyncini::core::keyOf(o);
  if(o.kind==Kind::Set)disk[key]=o.value.value;
  if(o.kind==Kind::DeleteKey)disk.erase(key);
  return Result{};
 });
 // A controlled slow writer exposes admission/coalescing without relying on
 // wall-clock disk speed. No native Windows file operation runs in this test.
 assert(store.write(L"Gate",L"Hold",L"1",L"Assist.ini"));gate.await();
 unsigned rejected=0,firstRejectedSave=0;
 for(unsigned save=1;save<=25;++save){
  const auto value=std::to_wstring(save);
  for(const auto&op:trace){
   if(!store.write(op.section,op.key,op.deleteKey?nullptr:value.c_str(),L"Assist.ini")){
    ++rejected;if(!firstRejectedSave)firstRejectedSave=save;
   }
  }
 }
 const auto before=store.status();
 gate.release();eventually([&]{return store.idle();});const auto drained=store.status();
 bool desiredVisible=true;
 for(const auto&op:trace)if(!op.deleteKey){
  const auto key=asyncini::core::keyOf(Operation(op.section,op.key,L"",L"Assist.ini"));
  const auto cached=store.lookup(op.section,op.key,L"Assist.ini");
  desiredVisible=desiredVisible&&(cached.known?cached.present&&cached.value==L"25":disk[key]==L"25");
 }
 assert(desiredVisible);
 const bool retried=store.retryFailed();eventually([&]{return store.idle();});
 assert(retried&&!store.status().failed);
 for(const auto&op:trace)if(!op.deleteKey){
  const auto key=asyncini::core::keyOf(Operation(op.section,op.key,L"",L"Assist.ini"));
  assert(disk[key]==L"25");
 }
 std::cout<<"{\"saves\":25,\"literal_operations_per_save\":"<<std::size(trace)
  <<",\"pending_before_release\":"<<before.pending<<",\"first_rejected_save\":"<<firstRejectedSave
  <<",\"rejected_operations\":"<<rejected<<",\"error_code\":"<<before.errorCode
  <<",\"failed_after_drain\":"<<(drained.failed?"true":"false")
  <<",\"latest_intent_preserved\":true,\"explicit_retry_succeeded\":true,\"legacy_precedence_checks\":5"
  <<",\"legacy_value_preserved\":"<<(disk.count(legacyKey)?"true":"false")<<",\"windows_executed\":false}"<<std::endl;
 return rejected||drained.failed?1:0;
}
'''


def sha(data):
    return hashlib.sha256(data).hexdigest()


def calls_from(sources):
    spans = (("assist/source/main.cpp", "static bool save(){", "static uint32_t flags(){"),
             ("assist/source/extra_settings_io.h", "static bool saveAdditionalSettings(){", "static std::string quotaID(){"))
    operations = []
    for name, start_marker, end_marker in spans:
        full = sources[name].decode()
        start, end = full.index(start_marker), full.index(end_marker)
        for match in CALL.finditer(full[start:end]):
            # The distributed unified Host does not compile this standalone key.
            if match.group(2, 3) == ("Experimental", "ScrollRefreshHz"):
                continue
            kind = "delete-key" if match.group(1) == "WritePrivateProfileStringW" and match.group(4).strip() == "nullptr" else "set"
            operations.append({"kind": kind, "section": match.group(2), "key": match.group(3),
                               "source": name, "line": full.count("\n", 0, start + match.start()) + 1})
    assert len(operations) >= 190, "Production save extraction unexpectedly shrank"
    return operations


def run(name, sources, output, compiler):
    folder = output / name
    folder.mkdir(parents=True, exist_ok=True)
    operations = calls_from(sources)
    load = re.search(r'\bextra\.manualReveal\s*=\s*value\(.*?;', sources["assist/source/extra_settings_io.h"].decode())
    assert load and 'L"ManualReveal"' in load.group(0) and 'L"RevealMap"' in load.group(0)
    (folder / "store.h").write_bytes(sources["shared/source/async_ini_store.h"])
    (folder / "test.cpp").write_text(FIXTURE)
    (folder / "manual-reveal.inc").write_text(load.group(0) + "\n")
    (folder / "trace.json").write_text(json.dumps(operations, ensure_ascii=False, indent=2) + "\n")
    (folder / "trace.inc").write_text("\n".join(
        "{L" + json.dumps(op["section"]) + ",L" + json.dumps(op["key"]) + "," +
        ("true" if op["kind"] == "delete-key" else "false") + "}," for op in operations) + "\n")
    command = [compiler, "-std=c++17", "-O2", "-pthread", str(folder / "test.cpp"), "-o", str(folder / "test")]
    built = subprocess.run(command, capture_output=True, text=True, timeout=60)
    (folder / "build.log").write_text(built.stdout + built.stderr)
    assert built.returncode == 0, str(folder / "build.log")
    executed = subprocess.run([str(folder / "test")], capture_output=True, text=True, timeout=30)
    (folder / "run.log").write_text(executed.stdout + executed.stderr)
    result = json.loads(executed.stdout)
    result.update(name=name, exit_code=executed.returncode,
                  source_sha256={name: sha(data) for name, data in sources.items()},
                  trace_sha256=sha((folder / "trace.json").read_bytes()),
                  delete_calls=sum(op["kind"] == "delete-key" for op in operations))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--out", type=Path)
    parser.add_argument("--baseline-revision", help="Explicit historical negative control, normally " + BASELINE)
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    output = args.out.resolve() if args.out else Path(tempfile.mkdtemp(prefix="assist-save-queue-"))
    output.mkdir(parents=True, exist_ok=True)
    sources = {name: (args.source / name).read_bytes() for name in SOURCE_FILES}
    current = run("current", sources, output, args.compiler)
    assert current["exit_code"] == 0 and current["delete_calls"] == 0
    assert current["pending_before_release"] <= current["literal_operations_per_save"]
    assert current["legacy_value_preserved"] and current["legacy_precedence_checks"] == 5
    assert all((args.source / name).read_bytes() == data for name, data in sources.items()), "Source changed during test"
    results = [current]
    if args.baseline_revision:
        old = {name: subprocess.check_output(["git", "-C", str(args.source.parent), "show",
                                               args.baseline_revision + ":source/" + name]) for name in SOURCE_FILES}
        baseline = run("baseline", old, output, args.compiler)
        assert baseline["exit_code"] == 1 and baseline["delete_calls"] == 1
        assert baseline["pending_before_release"] == 4096 and baseline["error_code"] == 8
        assert baseline["rejected_operations"] > 0 and baseline["failed_after_drain"]
        results.append(baseline)
    receipt = {"passed": True, "schema": "ra2.assist-save-queue.regression.v1", "runs": results,
               "baseline_revision": args.baseline_revision, "windows_executed": False,
               "scope": "Production Store with the source-ordered literal-key subset of both Assist save functions; controlled writer and values. Full save, Win32 profile I/O, file locks/ACL and the user machine were not executed."}
    (output / "receipt.json").write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(receipt, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
