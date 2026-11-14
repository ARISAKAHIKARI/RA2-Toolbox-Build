#!/usr/bin/env python3
"""Controlled execution of the real bounded missile-control delivery paths.

Production manual ownership, reset, submit, release, result admission, and
Runtime/Host dispatch are extracted verbatim. Raw/native observations and the
input collector are doubles; this is not Windows or in-game verification.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--source", type=Path)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
source = args.source or repo / "source/assist/source/missile_bridge.h"
raw = source.read_bytes()
text = raw.decode()
state = text[text.index("static compute::FrameHistory<missilecompute::"):text.index("static bool missileTarget(")]
delivery = text[text.index("static void pruneMissileSends("):text.index("// The producer has already sealed")]
dispatch = text[text.index("static bool missileControlTick("):]
observability = "" if "static bool missileManualOverflowed(" in state else "static bool missileManualOverflowed(){return false;}\n"
fixture = r'''
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <vector>
#include "compute_frame.h"
#include "compute_policy.h"
#include "missile_compute.h"
#include "plan_freshness.h"
#include "planner_types.h"
#include "object_index.h"
#include "unit_task_arbiter.h"
using namespace assist;
using DWORD=uint32_t;
struct Shared {
 bool missileEnabled=true;
 uint32_t missileComputed=0,missileScope=0,missileAllocation=0,missileReserve=0,lowDamageFilter=0;
 uint32_t missileSources=0,missileTargets=0,missileIssued=0,missileHeld=0,missileScanHz=60,computeApplied=0,missileTask=0;
 long missileResultSeq=0,missileInputSeq=0;
 missilecompute::Input missileInput;
 missilecompute::Result missileResult;
};
static Shared shared;static Shared*sh=&shared;
static ManualVersions manualVersions;
static uint32_t lastHouse=70000,match=1,clockTick=1000;
static int clockFrame=100,missileHoldCount=0,rosterReads=0,collections=0;
static bool sampleReady=true,sampleComplete=true,rosterReadable=true,queueAllowed=true,externalInputFrame=true;
static struct {bool chat=false;}snap;
struct {struct {uint32_t match=1;int frame=100;}key;}worldFrame;
static std::unordered_map<uint32_t,uint32_t> memory;
static std::vector<uint32_t> roster;
struct NativeEvent {uint32_t id;int action;};static std::vector<NativeEvent> events;
struct Vec{int count=0;};
static bool read(uint32_t at,void*out,size_t size){
 if(at==0xA8ED84&&size==4){std::memcpy(out,&clockFrame,4);return true;}
 auto it=memory.find(at);if(it==memory.end()){sampleComplete=false;return false;}
 std::memcpy(out,&it->second,size);return true;
}
template<class T>T get(uint32_t at){T value{};read(at,&value,sizeof value);return value;}
struct SampleReadScope {
 SampleReadScope(){sampleComplete=sampleReady;}
 explicit operator bool()const{return sampleReady;}
 bool complete()const{return sampleReady&&sampleComplete;}
};
static bool sampleRoster(uint32_t,Vec&v,int=16384){++rosterReads;if(!rosterReadable){sampleComplete=false;return false;}v.count=int(roster.size());return true;}
static uint32_t item(const Vec&,int i){return roster[size_t(i)];}
static uint32_t worldByID(uint32_t id){for(uint32_t p:roster)if(get<uint32_t>(p+0x10)==id)return p;return 0;}
static bool live(uint32_t p){return get<int>(p+0x6C)>0;}
static bool own(uint32_t p,uint32_t house){return live(p)&&get<uint32_t>(p+0x21C)==house;}
static int kind(uint32_t){return 2;}
static uint32_t typeOf(uint32_t,int){return 80000;}
static int remaining(int,int,int){return 0;}
static DWORD sampleClock(Shared*){return clockTick;}
static void rawCombatInfo(uint32_t,uint32_t,int,planner::Actor&a,bool,bool){a={};}
static bool missileTarget(uint32_t p,uint32_t){return p&&live(p);}
static bool missileSource(uint32_t,uint32_t);
static missilecompute::Edge missileEdge(uint32_t,uint32_t){missilecompute::Edge e{};e.damage=50;e.close=1;return e;}
static bool planningRoom(int){return true;}
static bool outgoingRoom(){return true;}
static void consumePlanning(int){}
static bool nativeMission(uint32_t,uint32_t id,int action,uint32_t,int,int,int,int,taskarbiter::Owner){if(!queueAllowed)return false;events.push_back({id,action});return true;}
static bool simulationReady(){return true;}
static void MemoryBarrier(){}
static void InterlockedIncrement(long*p){++*p;}
static void notifyHost(){}
'''
collector = r'''
static bool missileSource(uint32_t p,uint32_t house){return own(p,house)&&!missileManuallyOwned(p);}
static bool collectMissileInput(uint32_t,int frame,DWORD now,int&a,int&b,int&c){
 ++collections;a=b=c=0;missilecompute::reset(missileInput);missileInput.match=match;missileInput.frame=frame;missileInput.tick=now;return true;
}
'''
cases = r'''
static int checks=0,failures=0;
static void check(bool ok,const char*label){++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<label<<'\n';}}
static uint32_t unit(uint32_t id,int mission=2,uint32_t target=90000){
 uint32_t p=100000+id*1024;memory[p+0x10]=id;memory[p+0x6C]=100;memory[p+0x21C]=lastHouse;
 memory[p+0xAC]=uint32_t(mission);memory[p+0xB4]=uint32_t(-1);memory[p+0x2B4]=target;memory[p+0x5A4]=mission==2?91000:0;
 memory[p+0x2EC]=memory[p+0x2F4]=0;memory[p+0x9C]=memory[p+0xA0]=0;roster.push_back(p);return p;
}
static void reset(){
 memory.clear();roster.clear();events.clear();manualVersions.clear();++match;
 clockFrame=worldFrame.key.frame=100;worldFrame.key.match=match;clockTick=1000;
 sampleReady=sampleComplete=rosterReadable=queueAllowed=externalInputFrame=true;
 snap.chat=false;shared.missileEnabled=true;shared.missileComputed=shared.missileTask=0;
 shared.missileScope=shared.missileAllocation=shared.missileReserve=shared.lowDamageFilter=0;
 shared.computeApplied=0;shared.missileResultSeq=shared.missileInputSeq=0;
 resetMissileControl();missileSettings=0;rosterReads=collections=0;
}
static void fill(){for(uint32_t id=1;id<=1024;++id)pauseMissileForManual(id,unit(id));}
static void advance(int delta=1){clockFrame+=delta;worldFrame.key.frame=clockFrame;clockTick+=uint32_t(delta)*16;}
static void prepareResult(uint32_t actor,uint32_t target,int action){
 missilecompute::reset(missileInput);missileInput.match=match;missileInput.frame=clockFrame;missileInput.tick=clockTick;
 missileInput.count=missileInput.targetCount=1;missileInput.sources[0]={get<uint32_t>(actor+0x10),actor,manualVersions.get(get<uint32_t>(actor+0x10))};
 missileInput.targets[0]={get<uint32_t>(target+0x10),target,100};
 shared.missileResult={};shared.missileResult.match=match;shared.missileResult.frame=clockFrame;shared.missileResult.tick=clockTick;
 shared.missileResult.count=1;shared.missileResult.orders[0]={0,0,action};
}
int main(){
 reset();fill();advance();uint32_t fresh=unit(2000),target=unit(5000,1,0);
 pauseMissileForManual(2000,fresh);
 check(missileManualOverflowed(),"untracked manual input latches a visible automatic-fire pause");
 check(missileManualCount==1024,"saturation never evicts the 1024 active manual tasks");
 check(!submitMissile(fresh,1,5000,lastHouse,clockFrame,clockTick),"final native delivery gate blocks new Attack while saturated");
 check(!submitMissile(fresh,0,0,lastHouse,clockFrame,clockTick),"final native delivery gate blocks new Sleep while saturated");
 check(events.empty(),"failed manual registration cannot emit any new automatic command");
 events.clear();missileSendCount=0;prepareResult(fresh,target,1);
 applyMissileResult(shared.missileResult,lastHouse,clockFrame,clockTick);
 check(events.empty(),"already-computed Host Attack cannot cross the overflow latch");
 events.clear();missileSendCount=0;prepareResult(fresh,target,0);
 applyMissileResult(shared.missileResult,lastHouse,clockFrame,clockTick);
 check(events.empty(),"already-computed Host Sleep cannot cross the overflow latch");
 events.clear();missileSendCount=0;prepareResult(fresh,target,1);
 missileFrames.retain(1,missileInput);shared.missileComputed=1;
 missileControlTick(lastHouse,clockFrame,clockTick);
 check(events.empty()&&collections==0,"Host completion dispatch discards stale work and does not create another input");
 check(!missileFrames.find(1),"saturation drops retained result provenance");

 reset();uint32_t managed=unit(3000,5,0);fresh=unit(2000,5,0);
 check(submitMissile(managed,0,0,lastHouse,clockFrame,clockTick),"ordinary tool hold can be queued before saturation");
 check(submitMissile(fresh,0,0,lastHouse,clockFrame,clockTick),"preexisting hold exists for the later manually claimed actor");
 fill();advance();pauseMissileForManual(2000,fresh);events.clear();
 missileControlTick(lastHouse,clockFrame,clockTick);
 check(events.size()==1&&events[0].id==3000&&events[0].action==5,"only the still-tool-owned hold receives Guard during overflow");
 check(missileRecord(2000)<0,"a manual claim retires old tool ownership even without changing its token");
 check(missileManualOverflowed(),"release of owned holds does not silently clear the saturation latch");
 events.clear();advance(1000);missileControlTick(lastHouse,clockFrame,clockTick);
 check(missileManualOverflowed()&&collections==0,"elapsed time cannot guess that an untracked manual order has ended");
 shared.missileEnabled=false;missileControlTick(lastHouse,clockFrame,clockTick);advance(31);missileControlTick(lastHouse,clockFrame,clockTick);
 check(!missileSendCount&&!missileManualOverflowed(),"explicit disable clears the latch after existing ownership retires");
 shared.missileEnabled=true;events.clear();
 check(submitMissile(managed,1,5000,lastHouse,clockFrame,clockTick)&&events.size()==1,"explicit re-enable permits new automation again");

 reset();fill();advance();rosterReads=0;
 for(uint32_t id=2000;id<2020;++id)pauseMissileForManual(id,unit(id));
 check(rosterReads==1,"a same-frame full-cohort input attempts sealed-roster cleanup only once");
 check(missileManualOverflowed(),"further manual input preserves the paused state");
 ++match;worldFrame.key.match=match;resetMissileControl();
 check(!missileManualOverflowed()&&!missileManualCount,"world reset has no previous-match overflow state");
 events.clear();fresh=unit(6000,5,0);target=unit(7000,1,0);prepareResult(fresh,target,1);
 applyMissileResult(shared.missileResult,lastHouse,clockFrame,clockTick);
 check(events.size()==1&&events[0].action==1,"normal result delivery still works after world reset");

 reset();fill();advance();roster.clear();fresh=unit(2000);pauseMissileForManual(2000,fresh);
 check(!missileManualOverflowed()&&missileManualCount==1&&missileManuallyOwned(fresh),"complete dead-history reclamation avoids unnecessary saturation");
 reset();shared.missileEnabled=false;fill();
 check(!missileManualOverflowed()&&!missileManualCount,"disabled automation cannot latch an unused manual ledger");
 std::cout<<(failures?"FAIL: ":"PASS: ")<<checks<<" checks, "<<failures<<" failures; exact production ownership/delivery/Host dispatch paths\n";
 return failures?1:0;
}
'''
args.output.mkdir(parents=True, exist_ok=True)
generated = args.output / "missile_manual_capacity.cpp"
actual = state + delivery + dispatch
if "missilecompute::Observation" in state:
    cases=cases.replace("missileFrames.retain(1,missileInput);", "missilecompute::observe(missileObservation,missileInput);missileFrames.retain(1,missileObservation);")
    collector=collector.replace("missileInput.tick=now;return true;", "missileInput.tick=now;missilecompute::observe(missileObservation,missileInput);return true;")
    cases=cases.replace("missileInput.targets[0]={get<uint32_t>(target+0x10),target,100};", "missileInput.targets[0]={get<uint32_t>(target+0x10),target,100};missilecompute::observe(missileObservation,missileInput);")
generated.write_text(fixture + "\n// BEGIN EXACT PRODUCTION STATE\n" + state + observability
                     + "\n// END EXACT PRODUCTION STATE\n" + collector
                     + "\n// BEGIN EXACT PRODUCTION DELIVERY AND DISPATCH\n" + delivery + dispatch
                     + "\n// END EXACT PRODUCTION DELIVERY AND DISPATCH\n" + cases)
binary = args.output / "missile_manual_capacity"
commands = [["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-I", str(repo / "source/assist/source"), str(generated), "-o", str(binary)], [str(binary)]]
records = []
for index, command in enumerate(commands):
    completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    log = args.output / ("build.log" if index == 0 else "run.log")
    log.write_text(completed.stdout)
    records.append({"argv": command, "cwd": str(Path.cwd()), "exit_code": completed.returncode,
                    "log": str(log), "log_sha256": hashlib.sha256(log.read_bytes()).hexdigest()})
    print(json.dumps({"step": index, "exit_code": completed.returncode, "output": completed.stdout}, ensure_ascii=False))
    if index == 0 and completed.returncode:
        break
summary = {"verification_level": "controlled_execution", "source": str(source),
           "source_sha256": hashlib.sha256(raw).hexdigest(),
           "test_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
           "generated_sha256": hashlib.sha256(generated.read_bytes()).hexdigest(),
           "extract_sha256": hashlib.sha256(actual.encode()).hexdigest(), "commands": records}
(args.output / "result.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
raise SystemExit(records[-1]["exit_code"])
