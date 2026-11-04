#!/usr/bin/env python3
"""Test the production sidebar refresh with a synthetic bounded catalogue.

Only request/refresh scheduling is executed. Factory and AddCameo effects are
explicit substitutes; this is not a game, UI rendering or multiplayer test.
"""
import argparse
from pathlib import Path
import subprocess


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zig", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    args = parser.parse_args()
    sidebar = (Path(__file__).resolve().parent.parent / "source/sidebar.h").read_text(encoding="utf-8")
    request = function(sidebar, "static void requestTechRefresh(")
    restore = function(sidebar, "static void restoreTech(")
    refresh = function(sidebar, "static void refreshTech(")
    code = r'''
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "technology_mode_policy.h"
using namespace assist;
using DWORD=uint32_t;
static constexpr uint32_t UnsafeTech=4096;
struct Options{uint32_t flags=UnsafeTech;} options;
static Options*sh=&options;
struct Type{int kind=40,index=0;};
struct Snapshot{int typeCount=0,techOpen=0,techError=0,techVisible=0,techUnavailable=0;Type types[512]{};} snap;
static uint8_t techAllowed[512]{};
static bool present[512]{},rejected[512]{},techPending=false,sitesOK=true;
static int calls[512]{},ensureCalls=0,additions=0,observations=0,siteCalls=0;
static unsigned techImplementation=0,techScanImplementation=UINT32_MAX;
static int techScanAt=0,techScanVisible=0,techScanUnavailable=0,techScanFrame=INT32_MIN;
static uint64_t techScanCatalogue=0,catalogueHash=1;
static DWORD techRefresh=0;
static int frame=100;
template<class T>static T get(uint32_t){return T(frame);}
static bool setTechSites(bool){++siteCalls;return sitesOK;}
static bool restoreTechData(){return true;}
static bool restoreTechParameters(){return true;}
static bool restoreTechEnhanceParameters(){return true;} // independent HW component off
static bool requestTechParameters(uint32_t){return true;}
static bool applyTechData(uint32_t){return true;}
static bool nativeTextTechRetired(){return true;}
static bool nativeTechRetired(){return true;}
static bool techTransitionProductionPending(uint32_t){return false;}
REQUEST
RESTORE
static bool hasCameo(int,int index){return present[index];}
static bool ensureTechCameo(int i,uint32_t){
 ++ensureCalls;++calls[i];if(!techAllowed[i]||rejected[i])return false;
 if(!present[i]){present[i]=true;++additions;}return true;
}
static void observeMasterCameos(){++observations;techPending=false;snap.techVisible=37;snap.techUnavailable=0;}
REFRESH
static void reset(unsigned mode){
 options={};snap={};std::memset(techAllowed,0,sizeof techAllowed);
 std::memset(present,0,sizeof present);std::memset(rejected,0,sizeof rejected);std::memset(calls,0,sizeof calls);
 techPending=false;sitesOK=true;ensureCalls=additions=observations=siteCalls=0;
 techImplementation=mode;techScanAt=techScanVisible=techScanUnavailable=0;
 techScanFrame=INT32_MIN;techScanImplementation=UINT32_MAX;techScanCatalogue=0;catalogueHash=1;techRefresh=0;frame=100;
 for(int i=0;i<512;++i)snap.types[i].index=i;
}
int main(){
 int checks=0,failures=0;
 auto check=[&](bool ok,const char*what){++checks;if(!ok){++failures;std::fprintf(stderr,"FAIL: %s\n",what);}};
 for(unsigned mode:{0u,1u,3u,4u}){
  reset(mode);snap.typeCount=80;int allowed=0,visible=0,unavailable=0;
  for(int i=0;i<snap.typeCount;++i){techAllowed[i]=(i%5)!=0;rejected[i]=(i%7)==0;
   if(techAllowed[i]){++allowed;if(rejected[i])++unavailable;else ++visible;}}
  requestTechRefresh();refreshTech(0x21000,1000);
  check(allowed>32&&visible>8,"fixture exceeds both old batch limits");
  check(ensureCalls==allowed&&calls[79]==1,"whole allowed catalogue visited in first call");
  check(additions==visible,"more than eight new cameos added in first call");
  check(snap.techVisible==visible&&snap.techUnavailable==unavailable,"final counts distinguish native rejection from excluded types");
  check(!techPending&&techScanAt==snap.typeCount,"no partial pass remains for later frames");
  check(snap.techOpen==int(mode)+1&&techImplementation==mode,"implementation mode remains unchanged");
  bool skipped=true;for(int i=0;i<snap.typeCount;++i)if(!techAllowed[i]&&calls[i])skipped=false;
  check(skipped,"excluded types do not reach factory/cameo work");
  const int firstCalls=ensureCalls,firstAdds=additions;
  refreshTech(0x21000,1001);
  check(ensureCalls==firstCalls&&additions==firstAdds,"same-frame repeated wake does no cameo work");
  ++frame;refreshTech(0x21000,2000);
  check(ensureCalls==2*allowed&&additions==firstAdds,"later maintenance completes full pass of existing cameos");
  check(!techPending&&snap.techVisible==visible,"maintenance finishes without partial state");
  // An explicit changed allow list in the same frame waits, then applies fully.
  techAllowed[0]=1;requestTechRefresh();refreshTech(0x21000,2001);
  check(calls[0]==0,"same-frame changed list does not bypass frame guard");
  ++frame;refreshTech(0x21000,2017);
  check(calls[0]==1&&snap.techUnavailable==unavailable+1&&!techPending,"changed list completes on next eligible frame");
 }
 reset(0);snap.typeCount=512;std::memset(techAllowed,1,sizeof techAllowed);
 requestTechRefresh();refreshTech(0x21000,1000);
 check(ensureCalls==512&&additions==512&&!techPending,"maximum bounded catalogue completes in one pass");
 reset(0);snap.typeCount=80;requestTechRefresh();refreshTech(0x21000,1000);
 check(!ensureCalls&&!snap.techVisible&&!snap.techUnavailable&&!techPending,"empty allowed list completes without additions");
 reset(1);snap.typeCount=80;sitesOK=false;requestTechRefresh();refreshTech(0x21000,1000);
 check(snap.techError==-11&&!snap.techOpen&&!techPending&&!ensureCalls,"failed patch setup admits no cameo work");
 reset(0);options.flags=0;snap.techOpen=1;requestTechRefresh();refreshTech(0x21000,1000);
 check(!snap.techOpen&&!techPending&&!ensureCalls,"disabled permission restores mode without additions");
 reset(2);snap.typeCount=80;std::memset(techAllowed,1,sizeof techAllowed);
 requestTechRefresh();refreshTech(0x21000,1000);
 check(observations==1&&!ensureCalls&&!additions&&!techPending,"master mode only observes native cameos");
 check(snap.techOpen==3&&snap.techVisible==37,"master mode keeps original status");
 std::printf("Technology full-pass refresh: %d checks, %d failures\n",checks,failures);
 return failures?1:0;
}
'''.replace("REQUEST", request).replace("RESTORE", restore).replace("REFRESH", refresh)
    args.build_dir.mkdir(parents=True, exist_ok=True)
    cpp = args.build_dir / "tech_refresh_full_pass.cpp"
    exe = args.build_dir / "tech_refresh_full_pass.exe"
    cpp.write_text(code, encoding="utf-8")
    subprocess.run([str(args.zig.resolve()), "c++", "-std=c++20", "-O0", "-fno-exceptions", "-fno-rtti", "-I"+str((Path(__file__).resolve().parents[1]/"source")), str(cpp.resolve()), "-o", str(exe.resolve())], check=True)
    subprocess.run([str(exe.resolve())], check=True)


if __name__ == "__main__":
    main()
