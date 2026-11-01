#!/usr/bin/env python3
"""Run production reset and visual/command readiness with counted substitutes."""
import argparse
from pathlib import Path
import re
import subprocess


def function(text, signature):
    start = text.index(signature)
    opening = text.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zig", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1] / "source"
    bridge = (source / "bridge.cpp").read_text(encoding="utf-8")
    placement = (source / "placement_bridge.h").read_text(encoding="utf-8")
    reset = function(bridge, "static void reset(bool retireHooks")
    gates = "\n".join(function(placement, name) for name in (
        "static bool riskRenderContextReady(", "static bool riskContextReady(",
        "static bool riskReady(", "static bool riskRenderReady("))
    fields = sorted(set(re.findall(r"snap\.(\w+)", reset + gates)))
    restore_names = sorted(set(re.findall(r"\b(restore\w+|resetHookReadiness)\(", reset)))
    reset_names = sorted(set(re.findall(r"\b(reset\w+|clearFlashes|clearOreOrders)\(", reset)) - {"reset", "resetHookReadiness"})
    code = r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
#include <map>
#include "world_lifecycle.h"
using namespace assist;using DWORD=uint32_t;
int retired=0,jobs=0,gameCalls=0;bool displayVisible=true,entryActive=true,fault=false;
struct Snapshot {FIELDS} snap;
struct Shared {unsigned stop=0,heartbeat=0;int clickHookStatus=0;struct {int request=0,ack=0,result=0,accepted=0,rejected=0;struct {int count=0;}intent;}pointer;} state,*sh=&state;
void MemoryBarrier(){}
struct Local {void clear(){++jobs;}}runtimeKeys,missionObserver,actorFrameArbiter,actorLeaseArbiter,loadLedger,pendingRepairs;
int enhancementChangedCatalogue=1,stickyCount=3,catalogueReady=1,catalogueHash=9,catalogueAt=8,catalogueDirty=0,nativeMegaQueueCallDepth=0,nativeObjectQueueCallDepth=0,indexedFrame=3,indexedCount=4,outgoingFrame=5,repairCount=2,repairAt=1,repairDone=0,repairAckRequest=7,repairAckPending=1,repairCalculationPending=1,productionBaseline=1,crateCount=2,cratesOn=1,autoWas=1,sceneResultSerial=1,lastIdle[3]{},techAllowed[8]{9};
RESTORE_STUBS
RESET_STUBS
void stickyTick(bool){++gameCalls;}
void clearNativeOrders(bool retireHooks){++jobs;if(retireHooks)++retired;}
RESET
worldlife::Tracker hookWorld;uint32_t lastHouse=0x300000;
std::map<uint32_t,uint8_t>memory;
template<class T>void put(uint32_t at,T v){const auto*p=(const uint8_t*)&v;for(unsigned i=0;i<sizeof v;++i)memory[at+i]=p[i];}
template<class T>T get(uint32_t at){T v{};auto*p=(uint8_t*)&v;for(unsigned i=0;i<sizeof v;++i)p[i]=memory[at+i];return v;}
namespace gameentry {enum class Client{Assist};bool active(Client){return entryActive;}}
namespace external {namespace display {struct State {bool visible()const{return displayVisible;}};template<class R>State inspect(R){return {};}}}
namespace eventdelivery {bool mayQueue(int mode){return mode==0||mode==3||mode==4||mode==5;}}
DWORD sampleClock(Shared*){return 1000;}
GATES
int main(){
 snap.techOpen=2;snap.nativeHookStatus=1;snap.csHookStatus=1;snap.inputPatchMask=17;state.clickHookStatus=1;
 reset(false);assert(retired==0&&gameCalls==0&&jobs>0&&stickyCount==0&&repairCount==0);
 assert(snap.techOpen==2&&techAllowed[0]==9&&snap.nativeHookStatus==1&&snap.csHookStatus==1&&snap.inputPatchMask==17&&state.clickHookStatus==1);
 reset();assert(retired>10&&gameCalls==1&&snap.techOpen==0&&techAllowed[0]==0);
 worldlife::Sample world;world.readable=true;world.scenario=0x100000;world.cells=0x200000;world.count=4096;world.frame=100;world.session=3;world.running=1;hookWorld.observe(world);
 put(0xA8B230,world.scenario);put(0x87F924,world.cells);put(0x87F928,world.count);put(0xA8B238,3);put(0xA8ED84,100);put(0xA8E9A0,uint8_t(1));put(0xA83D4C,lastHouse);snap.frame=100;snap.session=3;
 assert(riskReady(true)&&riskRenderReady(true));
 put(0xAC1198,lastHouse);assert(!riskReady(true)&&riskRenderReady(true));put(0xAC1198,uint32_t(0));
 for(unsigned offset:{0x1F5u,0x1F6u,0x1F8u}){put(lastHouse+offset,uint8_t(1));assert(!riskReady(true)&&riskRenderReady(true));put(lastHouse+offset,uint8_t(0));}
 put(0xA8E9A0,uint8_t(0));assert(!riskReady(true)&&!riskRenderReady(true));put(0xA8E9A0,uint8_t(1));
 displayVisible=false;assert(!riskRenderReady(true));displayVisible=true;
 put(0xA8B230,uint32_t(0x110000));assert(!riskRenderReady(true));put(0xA8B230,world.scenario);
 put(0xA8ED84,99);assert(!riskRenderReady(true));put(0xA8ED84,101);assert(riskRenderReady(true));
 sh->stop=1;assert(!riskReady(true)&&!riskRenderReady(true));
 std::puts("PASS: extracted production reset(false) clears jobs without restoration or game calls; reset() retires; visual readiness admits observer/defeat while command readiness rejects");
}
'''
    code = code.replace("FIELDS", "\n".join("int " + field + "=0;" for field in fields))
    code = code.replace("RESTORE_STUBS", "\n".join("void " + name + "(){++retired;}" for name in restore_names))
    code = code.replace("RESET_STUBS", "\n".join("void " + name + "(){++jobs;}" for name in reset_names))
    code = code.replace("RESET", reset).replace("GATES", gates)
    args.build_dir.mkdir(parents=True, exist_ok=True)
    cpp, exe = args.build_dir / "production.cpp", args.build_dir / "production.exe"
    cpp.write_text(code, encoding="utf-8")
    cmd = [str(args.zig), "c++", "-target", "x86_64-windows-gnu", "-std=c++17", "-O1", "-I", str(source), str(cpp), "-o", str(exe)]
    build = subprocess.run(cmd, text=True, capture_output=True)
    (args.build_dir / "build.log").write_text(build.stdout + build.stderr, encoding="utf-8")
    if build.returncode:
        print(build.stderr)
        raise SystemExit(build.returncode)
    result = subprocess.run([str(exe)], text=True, capture_output=True)
    (args.build_dir / "test.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout + result.stderr)
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
