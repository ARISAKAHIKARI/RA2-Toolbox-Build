#!/usr/bin/env python3
"""Execute extracted production enhancement world/restore/admission functions.

The production record templates and session-epoch tracker also run unchanged.
Reads/writes and catalogue refresh are explicit substitutes; no game executes.
"""
import argparse
import json
from pathlib import Path
import subprocess


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zig", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--bridge", type=Path, help="Optional old production source for the regression control")
    args = parser.parse_args()
    source_dir = Path(__file__).resolve().parents[1] / "source"
    bridge = (args.bridge or source_dir / "tech_enhancement_bridge.h").read_text(encoding="utf-8")
    globals_text = bridge[:bridge.index("static bool enhancementWrite(")]
    extracted = "\n".join(function(bridge, signature) for signature in (
        "static techenhancement::IdentityState enhancementWorld(",
        "static bool restoreTechEnhancement(",
        "static void techEnhancementTick("))
    code = r'''
#include <cstdint>
#include <climits>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#include "session_epoch.h"
using DWORD=uint32_t;
using namespace assist;
static sessionepoch::Tracker sessionTracker;
struct Settings {bool techEnhance=true;int techEnhanceStatus=0;unsigned techEnhanceCount=0;} options;
static Settings*sh=&options;
struct Snapshot {int techOpen=0;} snap;
static bool enhancementChangedCatalogue=false,catalogueDirty=false,catalogueReady=true;
static DWORD catalogueAt=0;
static unsigned scans=0,refreshes=0;
static bool techDataBlocksEnhancement(){return false;}
static void scanTypes(){++scans;}
static void requestTechRefresh(){++refreshes;}
GLOBALS
struct Memory {
 std::map<uint32_t,uint8_t> bytes;
 uint32_t unreadable=0;bool failWrite=false,failAfter=false;
 std::vector<int> values;
 template<class T>void set(uint32_t at,T value){const auto*p=(const uint8_t*)&value;for(unsigned i=0;i<sizeof value;++i)bytes[at+i]=p[i];}
 bool read(uint32_t at,void*out,size_t n){
  if(unreadable>=at&&unreadable<uint64_t(at)+n)return false;
  auto*p=(uint8_t*)out;for(unsigned i=0;i<n;++i){auto it=bytes.find(at+i);if(it==bytes.end())return false;p[i]=it->second;}return true;
 }
 int level(){int v=0;read(0x20634,&v,4);return v;}
} memory;
static bool read(uint32_t at,void*out,size_t n){return memory.read(at,out,n);}
template<class T>static T get(uint32_t at){T v{};read(at,&v,sizeof v);return v;}
struct Vec {uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
static Vec vec(uint32_t at,int){Vec v{};return read(at,&v,sizeof v)?v:Vec{};}
static uint32_t item(Vec v,int i){return get<uint32_t>(v.items+i*4);}
static bool enhancementWrite(uint32_t at,const void*p,size_t n){
 if(at!=0x20634||n!=4){std::fputs("unexpected write target\n",stderr);return false;}
 if(memory.failWrite&&!memory.failAfter)return false;
 int value=0;std::memcpy(&value,p,4);memory.set<int>(at,value);memory.values.push_back(value);return !memory.failWrite;
}
FUNCTIONS
static void reset(){
 // Start each independent case by finishing through the production restore.
 memory.unreadable=0;memory.failWrite=false;memory.set<int>(0x20634,-1);
 restoreTechEnhancement();memory={};sessionTracker={};options={};snap={};scans=refreshes=0;
 enhancementChangedCatalogue=catalogueDirty=false;catalogueReady=true;catalogueAt=0;
 for(auto&r:enhancedTypes)r={};
 memory.set<uint32_t>(0xA8B230,0x90000);memory.set<uint32_t>(0xA83D4C,0x10000);
 memory.set<int>(0xA8B238,3);memory.set<int>(0xA8ED84,100);
 memory.set<Vec>(0xA8E348,{0,0x30000,1,0,1,1});memory.set<uint32_t>(0x30000,0x20000);
 memory.set<Vec>(0xA83CE0,{0,0x31000,0,0,0,1});memory.set<Vec>(0xA8B218,{0,0x32000,0,0,0,1});
 memory.set<uint32_t>(0x20000,techenhancement::Rules[0].vtable);
 char id[24]{};std::memcpy(id,techenhancement::Rules[0].id,7);for(unsigned i=0;i<24;++i)memory.set<uint8_t>(0x20024+i,id[i]);
 memory.set<int>(0x20634,-1);sessionTracker.update(true,0x10000,3,100);
}
int main(){
 int checks=0,failures=0;auto check=[&](bool ok,const char*what){++checks;if(!ok){++failures;std::fprintf(stderr,"FAIL: %s\n",what);}};
 auto apply=[&](){techEnhancementTick(1000);check(enhancedTypes[0].owned&&memory.level()==1,"initial production admission owns original -1");};
 reset();apply();options.techEnhance=false;techEnhancementTick(1001);
 check(memory.level()==-1&&!enhancedTypes[0].owned&&!enhancementRestoring,"ordinary off restores original");

 // Actual tracker: a failed world/type-directory read makes outer Runtime
 // temporarily inactive. Cleanup waits; a later Start bumps command epoch.
 reset();apply();memory.unreadable=0xA8E34C;
 check(!restoreTechEnhancement()&&enhancementRestoring&&enhancedTypes[0].owned,"failed directory read keeps restoration journal");
 sessionTracker.update(false,0,-1,-1);memory.unreadable=0;memory.set<int>(0xA8ED84,101);
 sessionTracker.update(true,0x10000,3,101);options.techEnhance=false;techEnhancementTick(1001);
 check(memory.level()==-1&&!enhancedTypes[0].owned&&!enhancementRestoring,"same-world restart epoch restores rather than forgetting original");

 reset();apply();memory.set<uint32_t>(0xA83D4C,0x11000);sessionTracker.update(true,0x11000,3,101);memory.set<int>(0xA8ED84,101);
 const size_t beforeHouse=memory.values.size();techEnhancementTick(2000);
 check(memory.values.size()==beforeHouse+2&&memory.values[beforeHouse]==-1&&memory.values.back()==1,"House/epoch change restores before rebinding and reapplying");
 check(enhancementHouse==0x11000&&enhancementEpoch==sessionTracker.epoch&&enhancedTypes[0].owned,"new House binding has fresh original responsibility");
 options.techEnhance=false;techEnhancementTick(2001);check(memory.level()==-1,"off after House change still restores true original");

 reset();apply();memory.set<uint32_t>(0xA83D4C,0x11000);memory.set<int>(0xA8ED84,101);sessionTracker.update(true,0x11000,3,101);
 memory.failWrite=true;const size_t beforeBlocked=memory.values.size();techEnhancementTick(2000);
 check(enhancementRestoring&&enhancedTypes[0].owned&&enhancementHouse==0x10000&&memory.values.size()==beforeBlocked,"failed old-context restore blocks new context admission");
 memory.failWrite=false;techEnhancementTick(2001);
 check(enhancedTypes[0].owned&&enhancementHouse==0x11000&&memory.values[memory.values.size()-2]==-1,"retry restores old before admitting new");

 for(uint32_t at:{0xA8B230u,0xA8B238u,0xA8ED84u}){
  reset();apply();memory.unreadable=at;options.techEnhance=false;techEnhancementTick(1100);
  check(enhancementRestoring&&enhancedTypes[0].owned,"unreadable real world identity retains ownership");
  memory.unreadable=0;techEnhancementTick(1101);check(memory.level()==-1&&!enhancementRestoring,"world read retry restores original");
 }
 reset();memory.unreadable=0xA8B230;techEnhancementTick(1000);
 check(memory.values.empty()&&!enhancementRequested,"unreadable Scenario cannot authorize initial binding");
 memory.unreadable=0;techEnhancementTick(1001);check(enhancedTypes[0].owned,"readable world can authorize later");

 reset();apply();memory.set<uint32_t>(0xA8B230,0xA0000);size_t before=memory.values.size();
 check(restoreTechEnhancement()&&!enhancedTypes[0].owned&&memory.values.size()==before,"changed Scenario retires old world without writes");
 reset();apply();memory.set<int>(0xA8B238,5);before=memory.values.size();
 check(restoreTechEnhancement()&&memory.values.size()==before,"changed session retires old world without writes");
 reset();apply();memory.set<int>(0xA8ED84,200);techEnhancementTick(1001);memory.set<int>(0xA8ED84,150);before=memory.values.size();
 check(restoreTechEnhancement()&&memory.values.size()==before,"rewind after an intervening short tick never restores stale saved-state data");
 reset();apply();memory.set<uint32_t>(0x30000,0x21000);before=memory.values.size();
 check(restoreTechEnhancement()&&memory.values.size()==before,"replaced Type is not restored even in same world");
 reset();apply();memory.failWrite=true;memory.failAfter=true;options.techEnhance=false;techEnhancementTick(1001);
 check(enhancementRestoring&&enhancedTypes[0].owned&&memory.level()==-1,"failed verification after restore retains ownership");
 memory.failWrite=false;before=memory.values.size();techEnhancementTick(1002);
 check(!enhancementRestoring&&memory.values.size()==before,"already-restored retry is idempotent");
 std::printf("production enhancement-world checks=%d failures=%d; synthetic reads/writes; no game or multiplayer\n",checks,failures);
 return failures?1:0;
}
'''.replace("GLOBALS", globals_text).replace("FUNCTIONS", extracted)
    args.build_dir.mkdir(parents=True, exist_ok=True)
    cpp, exe = args.build_dir / "tech_enhancement_world.cpp", args.build_dir / "tech_enhancement_world.exe"
    cpp.write_text(code, encoding="utf-8")
    command = [str(args.zig), "c++", "-target", "x86_64-windows-gnu", "-std=c++17", "-O1", "-fno-exceptions", "-fno-rtti", "-I", str(source_dir), str(cpp), "-o", str(exe)]
    compiled = subprocess.run(command, text=True, capture_output=True)
    (args.build_dir / "build.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        print(compiled.stderr)
        raise SystemExit(compiled.returncode)
    result = subprocess.run([str(exe)], text=True, capture_output=True)
    (args.build_dir / "test.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    (args.build_dir / "result.json").write_text(json.dumps({"exit_code": result.returncode, "source": str(args.bridge or source_dir / "tech_enhancement_bridge.h"), "production_functions_extracted": True, "game_tested": False, "multiplayer_tested": False, "output": result.stdout + result.stderr}, indent=2), encoding="utf-8")
    print(result.stdout + result.stderr)
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
