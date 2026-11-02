"""Execute the production AirPathMove branch against a bounded fake world.

This tests dispatch admission, not a reimplementation of air_path_policy. No
game process, engine DLL or private executable is read or modified.
"""
import argparse
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--zig", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1] / "source"
    bridge = (source / "bridge.cpp").read_text(encoding="utf-8")
    begin = bridge.index("else if(c==AirPathMove){") + len("else if(c==AirPathMove)")
    end = bridge.index("\n else if(c==ProduceReserved)", begin)
    branch = bridge[begin:end].strip()
    assert branch.startswith("{") and branch.endswith("}")
    prelude = r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <utility>
#include "air_path_policy.h"
using namespace assist;
constexpr uint32_t AirWaypoints=0x20000000u, NativeAirWaypoints=0x80000000u;
struct Shared {uint32_t flags=NativeAirWaypoints, nativeAirNetwork=0, requestToken=7; int arg2=1001;} state;
Shared* sh=&state;
int session=3, missionCalls=0;
template<class T> T get(uint32_t address){return address==0xA8B238?T(session):T(0);}
uint32_t worldByID(uint32_t id){return id==42?0x10000:0;}
bool own(uint32_t p,uint32_t house){return p==0x10000&&house==0x20000;}
int kind(uint32_t){return 0;}
uint32_t readMapCell(int x,int y){return x==1&&y==1?0x30000:0;}
struct ManualVersions {uint32_t get(uint32_t){return 7;}} manualVersions;
namespace taskarbiter {enum class Owner { Generic };}
bool nativeMission(uint32_t,uint32_t,int,uint32_t,uint32_t,int,uint32_t,uint32_t,taskarbiter::Owner){++missionCalls;return true;}
std::pair<int,int> dispatch(){uint32_t house=0x20000;int arg=42,result=1,n=0;
'''
    postlude = r'''
return {result,n};}
int main(){
 int checks=0;
 auto check=[&](bool accepted){missionCalls=0;auto result=dispatch();
  assert(result.first==(accepted?1:-4));++checks;
  assert(result.second==(accepted?1:0));++checks;
  assert(missionCalls==(accepted?1:0));++checks;};
 for(int mode:{0,5,3,4}){session=mode;
  // Request was captured in event-compatible mode. Switch the shared setting
  // before Runtime consumes that already published mailbox request.
  state={};state.nativeAirNetwork=0;check(true);
  state.nativeAirNetwork=1;check(false);
  state.nativeAirNetwork=2;check(false);
  state.nativeAirNetwork=0;check(true);
  state.flags=0;check(false);
  // A custom held key explicitly selects the sampled event route.
  state.flags=AirWaypoints;state.nativeAirNetwork=2;check(true);
  state.requestToken=8;check(false);
 }
 session=2;state={};check(false);
 std::printf("PASS: %d production AirPathMove admission assertions; no game connected\n",checks);
}
'''
    args.build_dir.mkdir(parents=True, exist_ok=True)
    cpp = args.build_dir / "air-path-command-admission.cpp"
    exe = args.build_dir / "air-path-command-admission.exe"
    cpp.write_text(prelude + branch + postlude, encoding="utf-8")
    subprocess.run([str(args.zig.resolve()), "c++", "-target", "x86_64-windows-gnu",
                    "-std=c++17", "-O2", "-I", str(source), str(cpp.resolve()),
                    "-o", str(exe.resolve())], check=True)
    subprocess.run([str(exe.resolve())], check=True)


if __name__ == "__main__":
    main()
