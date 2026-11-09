#!/usr/bin/env python3
"""A/B the actual Host sampling, registration, and bridge-demand call sites.

Historical main.cpp bytes are frozen from GitHub and verified by blob/SHA-256.
The complete production sampleExternal, mergeExternal, command identity and
registration functions are compiled without rewriting their decisions. The
tick wrapper preserves the real order: registration first, sampling next,
needsBridge last. Win32, successful external reads, and IPC publication are
controlled boundaries. Runtime IPC becomes available only after ensureBridge
is called; the boundary deliberately makes connection/publication immediate.

This proves a cold dependency cycle in a specific non-default configuration.
It does not claim to reproduce the user's already-connected game log, execute
game commands, or reproduce Windows OS key delivery. Direct UI send() is out
of scope; unlike unregistered keys it can call ensureBridge itself.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def function(source: str, name: str, optional: bool = False) -> str:
    found = re.search(rf"static\s+[^;{{\n]*\b{re.escape(name)}\s*\([^;{{]*\)\s*\{{", source)
    if not found:
        if optional:
            return ""
        raise ValueError(f"missing production function: {name}")
    at = source.index("{", found.start())
    end, depth = at + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[found.start():end]


PREAMBLE = r'''
#include "runtime_feature_demand.h"
#include "control_link.h"
#include "hotkey_routes.h"
#include "type_hotkey.h"
#include "hook_policy.h"
#include "destruct_owner_input.h"
#include "placement_event_input.h"
#include <array>
#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
using namespace assist;
using DWORD=uint32_t;using WORD=uint16_t;using BYTE=uint8_t;using UINT=unsigned;using HWND=uintptr_t;
constexpr UINT MOD_ALT=1,MOD_CONTROL=2,MOD_SHIFT=4,MOD_NOREPEAT=0x4000;
constexpr unsigned HOTKEYF_SHIFT=1,HOTKEYF_CONTROL=2,HOTKEYF_ALT=4;
static unsigned LOBYTE(uintptr_t v){return v&255;}
static unsigned HIBYTE(uintptr_t v){return (v>>8)&255;}
static DWORD now=10000,lastData=0,pid=321;
static DWORD GetTickCount(){return now;}
struct LARGE_INTEGER{int64_t QuadPart=0;};
static void QueryPerformanceCounter(LARGE_INTEGER*p){p->QuadPart=int64_t(now)*1000;}
static void QueryPerformanceFrequency(LARGE_INTEGER*p){p->QuadPart=1000000;}
static bool enabled=true,focused=true,diagEnabled=false,infoAdaptive=false;
static bool debugPageActive=false,techMode=false,buildTimeTooltipEnabled=false,hasSelection=false;
static bool foregroundGame(){return focused;}
static bool secondaryDisplayAvailable(){return false;}
static int informationFPS(){return 100;}
static DWORD sampleRenderClock(void*){return now;}
static void critical(const std::wstring&){}static void diag(const std::wstring&){}static void notify(const std::wstring&){}
static void probeNativeViewportMapping(){}
static std::unique_ptr<Shared> storage=std::make_unique<Shared>();
static Shared* shared=nullptr;
static Snapshot snap{},incoming{},externalPublished{};
static const Snapshot*externalData=nullptr;
static AdditionalSettings extra;
static DWORD externalAt=0;
static bool externalSourceConfirmed=false;
static unsigned bridgeAttempts=0,samplerCalls=0,scopeAttempts=0,scopeFailures=0,wakes=0;
static uint32_t configuredFlags=0;
static uint32_t flags(){return configuredFlags;}
static uint32_t statusOptions(){return 0;}
static uint32_t commandMatch();static uint32_t displayMatch();
static bool commandChannelReady();static void mergeExternal();
static void wakeGame(){++wakes;}
static bool ensureBridge(){
 ++bridgeAttempts;shared=storage.get();shared->stop=0;
 incoming={};incoming.status=incoming.bridgeStatus=incoming.commandStatus=1;incoming.match=1;lastData=now;
 return true;
}
static struct{unsigned size=0;}commandQueue;
static struct{std::set<uint32_t>stopped;}plannerState;
namespace nativeviewhost{
struct Transform{uint32_t generation=1;};
static bool sampleTransform(Shared*,Transform&t){t={};return true;}
}
namespace pipelineperf{
struct Window{Window(const wchar_t*,std::initializer_list<const wchar_t*>){}};
struct Scope{template<class F>Scope(Window&,int,F){}};
}
struct Motion{void projection(uint32_t){}void clear(){}template<class T>void update(const T&,int){}};
static Motion motion,statusMotion,enemyMotion;
static std::map<uint32_t,int>labelRasters;
static void clearEnemyDisplay(){}static void clearPassengerDisplay(){}static void rememberLiveTypes(){}
static void updateEnemyDisplay(const Snapshot&){}static void updateStatusDisplay(const Snapshot&){}
static struct{uint64_t samples=0,paints=0,uploads=0,readUs=0,maxReadUs=0;DWORD since=0,maxGap=0;}displayCounters;
namespace worldframe{
enum{Base=1,Combat=2,Passengers=4};
struct Key{uint32_t pid=321,match=1,connectionLow=1,connectionHigh=0,tick=0,serial=0;int frame=0;
 bool sameWorld(const Key&k)const{return pid==k.pid&&match==k.match;}
 bool sameFrame(const Key&k)const{return sameWorld(k)&&frame==k.frame;}
};
struct Frame{Key key;};using Token=std::shared_ptr<const Frame>;
}
static worldframe::Token externalWorldSource;
struct ReaderBoundary{
 uint32_t errors=0,lastReadError=0,lastFailedAddress=0,lastFailedSize=0,lastBytesRead=0,pageHits=0,pageFallbacks=0,lastRegionState=0,lastRegionProtect=0,lastRegionQueryError=0;
 bool valid()const{return true;}void begin(){}void setWorldDemand(uint32_t){}
 uint32_t pid()const{return 321;}uint64_t generation()const{return 1;}
 bool readLive(uint32_t,void*out,size_t n){std::memset(out,0,n);return true;}
 bool readFresh(uint32_t p,void*out,size_t n){return readLive(p,out,n);}
 worldframe::Token worldFrameToken(){auto f=std::make_shared<worldframe::Frame>();f->key.tick=now;f->key.frame=int(now/100);f->key.serial=now;return f;}
};
static ReaderBoundary externalReader;
namespace external{
// This is the missing/available IPC boundary, not the production selection
// rule. The actual function decides whether and when to acquire it.
struct WorldFrameScope{
 bool bound;WorldFrameScope(ReaderBoundary&,uint32_t,worldframe::Token={}):bound(shared!=nullptr){++scopeAttempts;if(!bound)++scopeFailures;}
 explicit operator bool()const{return bound;}bool complete()const{return bound;}
};
namespace attacktrace{struct Recorder{template<class...T>void sample(T&&...){}};}
namespace watch{static bool firstBinding(const uint32_t*k,int slot){if(slot<0||slot>=3||!(k[slot]&255))return false;for(int n=0;n<slot;++n)if(k[n]==k[slot])return false;return true;}}
}
static external::attacktrace::Recorder attackRecorder;
struct SamplerBoundary{
 Snapshot value{};unsigned flashCount=0;
 void viewport(const nativeviewhost::Transform&,bool){}void stopped(const std::set<uint32_t>&){}
 void worldEpoch(uint32_t){}void commitSample(){}void invalidateSample(const Snapshot*p){if(p)value=*p;}
 const Snapshot&state()const{return value;}void refreshCamera(DWORD){}
 void reset(){value={};flashCount=0;}
 struct Aircraft{bool current(uint32_t,int,uint32_t,DWORD,bool)const{return true;}};
 Aircraft idleAircraftDisplay()const{return {};}
 bool refreshAircraft(DWORD,uint32_t){return true;}
 const Snapshot&sample(DWORD tick,uint32_t,bool,uint32_t,uint32_t=0){
  ++samplerCalls;value={};value.status=value.readStatus=value.renderStatus=1;value.match=1;value.frame=int(now/100);value.sampledTick=tick;value.width=1024;value.height=768;return value;
 }
 struct State{int tactical=1,specialDialog=0,dialogCount=0,readable=1;};State displayState(){return {};}
 struct Probe{int stage=1;uint32_t address=0,value=0;};Probe probe(){return {};}
};
static SamplerBoundary externalSampler;
static bool keysRegistered=false,hookSmartUnload=false,hookAutoCS=false,buildAnywhereMode=false;
static int registeredHotkeyMode=0;static HWND ui=1;
static WORD hotkeys[12]{},ejectKey=0,unloadKey=0,csKey=0,idleAircraftKey=0,socialKeys[9]{};
struct Binding{std::string id;WORD key;};static std::vector<Binding>swBindings,typeBindings;
static typekeys::Tap typeTap,variantTap,formationTap;
static hotkeyroute::Registry hotkeyRegistry;static hotkeyroute::Pending hotkeyPending;
static std::vector<int>registeredKeyIDs,osRegistered;
static short GetAsyncKeyState(int){return 0;}
static bool RegisterHotKey(HWND,int id,UINT,unsigned){osRegistered.push_back(id);return true;}
static bool UnregisterHotKey(HWND,int id){osRegistered.erase(std::remove(osRegistered.begin(),osRegistered.end(),id),osRegistered.end());return true;}
static DWORD GetLastError(){return 0;}
static bool destructOwnerKeyConflict(WORD){return false;}
struct HotkeyDiagnostics{uint32_t messages=0,sampled=0,actions=0,blocked=0,fallbacks=0,lastError=0;};
static HotkeyDiagnostics hotkeyDiagnostics;
static void logHotkeyInput(const wchar_t*,int=0,uint16_t=0,DWORD=0){}
'''

TAIL = r'''
static unsigned checks=0;
static void check(bool okay,const char*name){++checks;if(!okay)throw std::runtime_error(name);}
static void reset(const std::string&scenario){
 now+=20000;lastData=0;shared=nullptr;snap={};incoming={};externalPublished={};externalData=nullptr;externalAt=0;externalSourceConfirmed=false;externalWorldSource.reset();externalSampler.reset();
 bridgeAttempts=samplerCalls=scopeAttempts=scopeFailures=wakes=0;configuredFlags=0;
 enabled=focused=true;debugPageActive=techMode=buildTimeTooltipEnabled=hasSelection=false;
 commandQueue.size=0;extra={};extra.missionModeKeys[selectedmode::OwnGround]=0;
 hotkeys[11]=WORD('L'|((HOTKEYF_CONTROL|HOTKEYF_ALT)<<8));
 keysRegistered=false;registeredHotkeyMode=0;registeredKeyIDs.clear();osRegistered.clear();hotkeyRegistry.clear();hotkeyPending.clear();
 if(scenario=="default_own_ground")extra=AdditionalSettings{};
 if(scenario=="auto_repair_demand")configuredFlags=AutoRepair;
 if(scenario=="already_connected")ensureBridge();
 check(hotkeys[11]==0x064c,"AutoBoard remains bound to Ctrl+Alt+L");
 check(!extra.enemyMarkers&&!extra.watchSecondary&&!buildTimeTooltipEnabled,"no hidden display/tooltip opt-in");
 const bool expectedDemand=scenario=="default_own_ground"||scenario=="auto_repair_demand";
 check(runtimeFeatureDemand(flags(),extra,techMode,buildTimeTooltipEnabled)==expectedDemand,"actual production runtimeFeatureDemand matches preconditions");
}
static void publishCommandBoundary(){
 if(!shared)return;incoming.status=incoming.bridgeStatus=incoming.commandStatus=1;incoming.match=1;incoming.chat=0;lastData=now;snap=incoming;mergeExternal();
}
int main(){
 try{
  for(const std::string scenario:{"manual_only","default_own_ground","auto_repair_demand","already_connected"}){
   reset(scenario);
   for(unsigned tick=0;tick<12;++tick){now+=100;oneTick();publishCommandBoundary();}
   const bool expectedReady=!REGRESSED_SOURCE||scenario!="manual_only";
   const bool autoBoardRegistered=std::find(osRegistered.begin(),osRegistered.end(),1011)!=osRegistered.end();
   check((bridgeAttempts>0)==expectedReady,"bridge connection decision");
   check((externalData!=nullptr)==expectedReady,"display publication decision");
   check((registeredHotkeyMode==2)==expectedReady,"command registration mode");
   check(autoBoardRegistered==expectedReady,"real registerKeys includes AutoBoard only after mode admission");
   check((samplerCalls>0)==expectedReady,"raw sampler admission");
   if(!expectedReady)check(scopeFailures==12&&bridgeAttempts==0,"missing Runtime IPC loops without any connection attempt");
   std::cout<<"{\"scenario\":\""<<scenario<<"\",\"ticks\":12,\"feature_demand\":"<<runtimeFeatureDemand(flags(),extra,techMode,buildTimeTooltipEnabled)<<",\"bridge_attempts\":"<<bridgeAttempts<<",\"world_scope_attempts\":"<<scopeAttempts<<",\"world_scope_failures\":"<<scopeFailures<<",\"sampler_calls\":"<<samplerCalls<<",\"external_published\":"<<(externalData!=nullptr)<<",\"command_ready\":"<<commandChannelReady()<<",\"mode\":"<<registeredHotkeyMode<<",\"autoboard_registered\":"<<autoBoardRegistered<<"}\n";
  }
  std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"scenarios\":4}\n";
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<" after "<<checks<<" checks\n";return 2;}
}
'''


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    out = args.out or root / "results"
    out.mkdir(parents=True, exist_ok=True)
    source = args.repo / "source/assist/source"
    historical = json.loads((root / "historical_sources.json").read_text())
    for entry in historical:
        path = root / "sources" / entry["version"] / Path(entry["path"]).name
        raw = path.read_bytes()
        assert sha(path) == entry["sha256"]
        assert hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest() == entry["github_blob_sha"]
    for name in ("runtime_feature_demand.h", "additional_settings.h"):
        assert (source / name).read_bytes() == (root / "sources/v1.0.43" / name).read_bytes(), f"common header drift: {name}"
    versions = json.loads((root / "versions.json").read_text())
    rows = []
    funcs = ["commandLink", "commandMatch", "displayMatch", "commandChannelReady", "commandHotkeyContext", "desiredHotkeyMode", "modifiers", "synchronizeHotkeyKeys", "registerKey", "registerKeys", "discardExternalWorld", "mergeExternal", "sampleExternal"]
    for version in versions:
        path = root / "sources" / version["version"] / "main.cpp"
        assert sha(path) == version["sha256"]
        text = path.read_text()
        parts = [function(text, name, name in {"commandHotkeyContext", "discardExternalWorld"}) for name in funcs]
        # These exact three Host statements are extracted with their original
        # expression. No demand condition is restated inside the fixture.
        found = re.search(r"sampleExternal\(now\);uint32_t configured=flags\(\);\n bool needsBridge=[^\n]+\n if\(needsBridge&&!shared\)ensureBridge\(\);", text)
        assert found, version
        registration = "if(registeredHotkeyMode!=desiredHotkeyMode())registerKeys();"
        assert registration in function(text, "tick")
        callsite = "static void oneTick(){\n" + registration + "\n" + found.group() + "\n}\n"
        cpp = PREAMBLE + "\n" + "\n\n".join(parts) + "\n" + callsite + TAIL
        item = out / version["version"]
        item.mkdir(parents=True, exist_ok=True)
        (item / "production.cpp").write_text(cpp)
        (item / "callsite.cpp").write_text(callsite)
        compile_cmd = ["g++", "-std=c++17", "-O0", "-Wall", "-Wextra", "-Wno-unused-function", "-Wno-unused-variable", "-Wno-misleading-indentation", "-MMD", "-MF", str(item / "dependencies.d"), "-DRA2_UNIFIED=1", "-DREGRESSED_SOURCE=" + str(int(version["regressed"])), "-I", str(source), str(item / "production.cpp"), "-o", str(item / "production")]
        build = subprocess.run(compile_cmd, text=True, capture_output=True)
        (item / "compile.log").write_text(build.stdout + build.stderr)
        if build.returncode:
            raise RuntimeError(f"compile failed: {version['version']}: {item / 'compile.log'}")
        run = subprocess.run([str(item / "production")], text=True, capture_output=True)
        (item / "run.log").write_text(run.stdout + run.stderr)
        if run.returncode:
            raise RuntimeError(f"assertion failed: {version['version']}: {run.stderr}")
        records = [json.loads(line) for line in run.stdout.splitlines()]
        extraction = {name: hashlib.sha256(part.encode()).hexdigest() for name, part in zip(funcs, parts) if part}
        dependencies = (item / "dependencies.d").read_text().replace("\\\n", " ").split(":", 1)[1].split()
        dependency_hashes = {str(Path(dep).resolve()): sha(Path(dep)) for dep in dependencies if Path(dep).resolve().is_relative_to(args.repo.resolve())}
        rows.append({**version, "cases": records[:-1], "summary": records[-1], "extracted_functions_sha256": extraction, "callsite_sha256": hashlib.sha256(callsite.encode()).hexdigest(), "generated_cpp_sha256": sha(item / "production.cpp"), "production_header_dependencies": dependency_hashes, "compile_command": compile_cmd, "compile_exit": build.returncode, "run_exit": run.returncode, "run_log": str(item / "run.log"), "run_log_sha256": sha(item / "run.log")})
        print(version["version"], json.dumps(records[-1]), "manual_only:", json.dumps(records[0]))
    result = {"passed": True, "regression_reproduced": True, "first_introducing_commit": "944586abf9cd331d2aaf9fe24af744a593bd31d3", "scope": __doc__, "fixture_sha256": sha(Path(__file__)), "historical_sources": historical, "versions": rows}
    (out / "result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")


if __name__ == "__main__":
    main()
