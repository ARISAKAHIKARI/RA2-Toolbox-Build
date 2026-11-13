#!/usr/bin/env python3
"""Production FPS admission/maintenance/retirement at the read-only boundary.

Runs exact extracted production functions with controlled Win32, memory reads,
signature acceptance and synchronous hardware-service boundaries. Does not prove
real Windows/game/hardware operation. An optional fixed old-header control must
compile and fail at the named admission/retirement assertions.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess

FIXTURE = Path(__file__).resolve().parent / "fixtures/entry_pre1475/single_player_fps_runtime.h"
FIXTURE_SHA = "3e4d867c0586b96bdb59eae730784d90607a9ea86a453411fd40d6ac16a8f39d"
EXPECTED = [
    "FAIL readonly request immediately closes FPS admission",
    "FAIL readonly request immediately closes FPS callback",
    "FAIL retirement maintenance must not rearm FPS",
    "FAIL FPS retirement permits observation acknowledgement",
    "FAIL unacknowledged rapid resume keeps FPS fenced",
    "FAIL pending retirement cannot acquire FPS tag",
    "FAIL Runtime active retirement keeps FPS fenced after boolean clear",
    "FAIL Runtime active retirement cannot rearm FPS"
]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def function(text, signature):
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def run(root, out, old_header=None):
    out.mkdir(parents=True, exist_ok=True)
    snapshot = out / "snapshot"
    sources = {}

    def freeze(path):
        path = path.resolve()
        rel = str(path.relative_to(root))
        if rel in sources:
            return
        data = path.read_bytes()
        sources[rel] = sha(data)
        target = snapshot / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        for name in re.findall(rb'^\s*#\s*include\s*"([^"]+)"', data, re.M):
            child = path.parent / name.decode()
            if child.is_file():
                freeze(child)

    for name in ("protocol.h", "single_player_fps.h", "technology_mode_policy.h", "control_mode_policy.h"):
        freeze(root / "source/assist/source" / name)
    runtime_path = root / "source/assist/source/single_player_fps_runtime.h"
    runtime_data = old_header if old_header is not None else runtime_path.read_bytes()
    sources[str(runtime_path.relative_to(root))] = sha(runtime_data)
    (out / "single_player_fps_runtime.source").write_bytes(runtime_data)
    tech_path = root / "source/assist/source/tech_ui_breakpoint_bridge.h"
    tech_data = tech_path.read_bytes()
    sources[str(tech_path.relative_to(root))] = sha(tech_data)
    (out / "tech_ui_breakpoint_bridge.source").write_bytes(tech_data)
    runtime = runtime_data.decode().replace("\r\n", "\n")
    tech = tech_data.decode().replace("\r\n", "\n")
    fps = "\n".join(function(runtime, signature) for signature in (
        "static void disable()", "static bool fresh()", "static bool live()",
        "static void maintain()", "static void release()"))
    config = "\n".join(function(tech, signature) for signature in (
        "static bool groupOff(", "static bool configure()"))
    maintenance = "\n".join(function(tech, signature) for signature in (
        "static bool singlePlayerFPSRetired()", "static bool restoreSinglePlayerFPS()",
        "static void maintainTechParameters()"))
    extracted = {"fps": sha(fps.encode()), "configure": sha(config.encode()), "maintenance": sha(maintenance.encode())}
    cpp = r'''
#include "source/assist/source/protocol.h"
#include "source/assist/source/single_player_fps.h"
#include "source/assist/source/technology_mode_policy.h"
#include "source/assist/source/control_mode_policy.h"
#include <memory>
#include <iostream>
using namespace assist;
using DWORD=uint32_t;using HANDLE=void*;
struct LARGE_INTEGER {long long QuadPart=0;};
constexpr int FALSE=0,TIMER_ALL_ACCESS=1;
static DWORD observedNow=1000;
static auto state=std::make_unique<Shared>();static Shared*sh=state.get();
static bool fault=false;static readonly::RuntimeState readOnlyState;
static uint32_t lastHouse=0;static int techImplementation=0;
static DWORD GetTickCount(){return observedNow;}
static DWORD sampleClock(Shared*){return observedNow;}
static void MemoryBarrier(){}
template<class T>static T get(uint32_t at){
 if(at==0xA8B238)return T(0);if(at==0xA8B230)return T(0x10000);
 if(at==0xA8ED84)return T(100);if(at==0xA8E9A0)return T(1);return T(0);
}
template<class T,class U>static void InterlockedExchange(T*p,U v){*p=T(v);}
static bool QueryPerformanceFrequency(LARGE_INTEGER*p){p->QuadPart=1000000;return true;}
static HANDLE CreateWaitableTimerExW(void*,void*,unsigned,unsigned){return reinterpret_cast<HANDLE>(1);}
static HANDLE CreateWaitableTimerW(void*,int,void*){return reinterpret_cast<HANDLE>(1);}
static void CloseHandle(HANDLE){}
namespace assist {namespace techbp {enum{Signature=-1,System=-2,Slots=-3};constexpr uint32_t AllTags=1023u;}}
namespace assist {namespace mapvisibility {constexpr uint32_t Tag=2u;}}
namespace assist {namespace triggerinput {constexpr uint32_t Tag=64u;}}
namespace assist {namespace privateout {constexpr uint32_t BreakpointTags=896u;}}
namespace assist {static uint32_t privateMode=0;static bool privateEnabled=false;static void maintainTriggerInput(){}}
namespace assist {namespace singlefps {
static Clock clock;static HANDLE timer;static uint64_t frequency;
static uint32_t scenario;static int lastFrame=-1;
static bool desired=false,enabled=false;static int failure=0;
static bool compatible(bool=true){return true;}
''' + fps + r'''
}}
namespace assist {namespace techparameters {
struct Policy {long enabled=0,enhanced=0,controlling=0,showing=0;} policy;
static bool desired=false,enhancementDesired=false,controlDesired=false,mapDesired=false;
static bool enhancementCameoBlocked=false,controlEligible=false,triggerDesired=false,triggerEnabled=false,privateDesired=false;
static int triggerFailure=0,privateFailure=0;
static int failure=0,enhancementFailure=0,controlFailure=0,mapFailure=0;
struct Retry {void reset(){}void defer(){}} fullFailureRetry,controlFailureRetry;
static uint32_t FullGroup=1,resourceGroups=0,refusedGroups=0,attempted=0;
static DWORD retryAt=0,auditAt=0;
static int sites[10]{},callback=0;
struct Service {
 struct Lease {uint32_t mask=0;bool uncertain=false;uint32_t tags()const{return mask;}} lease;
 int state=1;
 void poll(){} bool pending()const{return false;}bool pendingFor(uint32_t)const{return false;}
 bool resources()const{return lease.mask!=0;}uint32_t activeTags()const{return lease.mask;}
 void configure(const int*,uint32_t tags,int){lease.mask=tags;}
 void retire(){lease.mask=0;}void audit(){}void inspect(){}
} service;
static void publish(){}static bool healthy(){return true;}
static bool ready(uint32_t mask){return (service.activeTags()&mask)==mask;}
static uint32_t enhancementTags(){return 4;}
static void syncDebugBreakpointPolicy(){}
static bool refreshControlEligibility(){return false;}
''' + config + r'''
}}
''' + maintenance + r'''
static unsigned checks=0,failures=0;
static void check(bool value,const char*text){++checks;if(!value){++failures;std::cout<<"FAIL "<<text<<'\n';}}
static void resetCase(){
 state=std::make_unique<Shared>();sh=state.get();sh->heartbeat=observedNow;fault=false;readOnlyState={};
 singlefps::release();techparameters::service.retire();
 techparameters::attempted=techparameters::resourceGroups=techparameters::refusedGroups=0;
 techparameters::retryAt=techparameters::auditAt=0;
}
int main(){
 resetCase();maintainTechParameters();
 check(!singlefps::desired&&singlePlayerFPSRetired(),"default FPS zero stays disabled");
 sh->singlePlayerFPS=60;maintainTechParameters();
 check(singlefps::fresh()&&singlefps::live(),"normal requested FPS has fresh live admission");
 check(singlefps::desired&&singlefps::enabled&&!singlePlayerFPSRetired(),"normal FPS can acquire its service tag");

 readonly::request(*sh,MemoryBarrier);
 check(!singlefps::fresh(),"readonly request immediately closes FPS admission");
 check(!singlefps::live(),"readonly request immediately closes FPS callback");
 restoreSinglePlayerFPS();maintainTechParameters();
 check(!singlefps::desired&&singlePlayerFPSRetired(),"retirement maintenance must not rearm FPS");
 auto retire=[](){restoreSinglePlayerFPS();maintainTechParameters();return singlePlayerFPSRetired();};
 auto cancel=[](){readonly::cancelMailboxes(*sh,MemoryBarrier);};
 auto admitted=readOnlyState.service(*sh,cancel,retire,MemoryBarrier);
 check(admitted==readonly::Admission::Observe&&readonly::settled(*sh),"FPS retirement permits observation acknowledgement");

 resetCase();sh->singlePlayerFPS=60;sh->readOnlyRequest=1;
 check(!singlefps::fresh(),"unacknowledged rapid resume keeps FPS fenced");
 maintainTechParameters();check(!singlefps::desired&&singlePlayerFPSRetired(),"pending retirement cannot acquire FPS tag");

 resetCase();sh->singlePlayerFPS=60;sh->readOnlyRequest=sh->readOnlyAck=1;readOnlyState.active=true;
 check(!singlefps::fresh(),"Runtime active retirement keeps FPS fenced after boolean clear");
 maintainTechParameters();check(!singlefps::desired&&singlePlayerFPSRetired(),"Runtime active retirement cannot rearm FPS");
 readOnlyState.active=false;maintainTechParameters();
 check(singlefps::fresh()&&singlefps::live()&&singlefps::enabled,"settled normal resume restores requested FPS");
 sh->singlePlayerFPS=0;maintainTechParameters();
 check(!singlefps::desired&&singlePlayerFPSRetired(),"explicit FPS off still retires service");

 resetCase();sh->singlePlayerFPS=60;sh->stop=1;
 check(!singlefps::fresh(),"stop still closes FPS admission");
 sh->stop=0;fault=true;check(!singlefps::fresh(),"fault still closes FPS admission");
 fault=false;sh->heartbeat=observedNow-501;check(!singlefps::fresh(),"stale heartbeat still closes FPS admission");
 std::cout<<"{\"scenarios\":6,\"checks\":"<<checks<<",\"failures\":"<<failures<<"}\n";
 return failures?1:0;
}
'''
    (out / "production.cpp").write_text(cpp)
    cmd = ["g++", "-std=c++17", "-O0", "-I", str(snapshot), str(out / "production.cpp"), "-o", str(out / "production")]
    compiled = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / "compile.log").write_text(compiled.stdout)
    result = {"scope": __doc__, "sources": sources, "extracted_source_sha256": extracted,
              "generated_cpp_sha256": sha(cpp.encode()), "compile_argv": cmd, "compile_exit": compiled.returncode}
    if compiled.returncode == 0:
        ran = subprocess.run([str(out / "production")], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (out / "run.log").write_text(ran.stdout)
        result.update(run_exit=ran.returncode, execution=json.loads(ran.stdout.splitlines()[-1]), raw_run_sha256=sha(ran.stdout.encode()))
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--negative-control", action="store_true")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    root = args.root.resolve()
    current = run(root, args.out / "current")
    report = {"schema": 1, "suite": "fps_read_only_recovery", "scope": current["scope"], "current": current, "sources": dict(current["sources"])}
    report["passed"] = current["compile_exit"] == 0 and current.get("run_exit") == 0 and current.get("execution", {}).get("checks") == 16 and current["execution"]["failures"] == 0
    if args.negative_control:
        data = FIXTURE.read_bytes()
        if sha(data) != FIXTURE_SHA:
            raise SystemExit("Pre-fence FPS fixture SHA-256 mismatch")
        negative = run(root, args.out / "negative-control", data)
        log = (args.out / "negative-control/run.log").read_text() if negative["compile_exit"] == 0 else ""
        observed = [line for line in log.splitlines() if line.startswith("FAIL ")]
        ok = negative["compile_exit"] == 0 and negative.get("run_exit") == 1 and negative.get("execution", {}).get("checks") == 16 and negative["execution"]["failures"] == len(EXPECTED) and observed == EXPECTED
        report["negative_control"] = {"passed": ok, "fixture_sha256": FIXTURE_SHA, "provenance_commit": "18af5094f1cbd955ebaac9fbe3c85a0682ef7ce9", "expected_failures": EXPECTED, "observed_failures": observed, "result": negative}
        report["sources"][str(FIXTURE.relative_to(root))] = FIXTURE_SHA
        report["passed"] = report["passed"] and ok
    script = Path(__file__).resolve()
    report["sources"][str(script.relative_to(root))] = sha(script.read_bytes())
    (args.out / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"suite": report["suite"], "passed": report["passed"], "checks": current.get("execution", {}).get("checks"), "failures": current.get("execution", {}).get("failures"), "negative_control": report.get("negative_control", {}).get("passed")}))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
