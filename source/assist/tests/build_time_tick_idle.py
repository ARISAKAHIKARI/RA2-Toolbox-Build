"""Execute real production tooltip lifecycle functions with a cheap native stub.

The production functions (not a rewritten policy model) are extracted and
compiled as C++17. This verifies that the disabled feature's 8 ms game-thread
tick does not repeatedly enter the native owner, while enable/restore, policy
changes and failed installs still retire the exact owner claim.
"""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
header=(root/"source/build_time_tooltip.h").read_text()

def take(signature):
    at=header.index(signature)
    opening=header.index("{",at)
    level=1
    pos=opening+1
    while level:
        if pos>=len(header):raise AssertionError("Unbalanced production function")
        level+=(header[pos]=="{")-(header[pos]=="}")
        pos+=1
    return header[at:pos]

functions=[
    take("static bool buildTimeTooltipCompatible(){"),
    take("static void buildTimeTooltipTick(){"),
    take("static void restoreBuildTimeTooltip(){"),
]
code=r"""
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstring>
using DWORD=uint32_t;
struct Shared {bool stop=false,buildTimeTooltip=false; int buildTimeTooltipStatus=0;};
Shared instance{};Shared*sh=&instance;
struct Snapshot {int status=1;} snap;
uint32_t match=1,now=1000;
bool fault=false;
unsigned nativeCalls=0,nativeFailure=0,patternCalls=0;
bool nativeWanted=false,permitPatterns=true;
static uint32_t sampleClock(Shared*){return now;}
static bool read(uint32_t,void*,size_t){return true;}
namespace debugentries {
 enum class Kind{Call};
 uint32_t policyRevision=1;
 static bool pattern(Kind,uint32_t,const uint8_t*,unsigned,bool(*)(uint32_t,void*,size_t)){
  ++patternCalls;return permitPatterns;
 }
}
extern "C" bool NativeTooltipBuildTime(bool wanted){
 ++nativeCalls;nativeWanted=wanted;
 return !wanted||!nativeFailure;
}
static bool buildTimeTooltipActive=false,buildTimeTooltipFailed=false,buildTimeTooltipClaimed=false;
static uint32_t buildTimeTooltipFailedMatch=0;
static void restoreBuildTimeTooltip();
"""
code+="\n".join(functions)
code+=r"""
int main(){
 // Idle disabled feature stays on the fast path across 1,000 game wakes.
 for(unsigned i=0;i<1000;++i){now+=8;buildTimeTooltipTick();}
 if(nativeCalls||patternCalls||instance.buildTimeTooltipStatus){puts("idle calls regression");return 1;}
 instance.buildTimeTooltip=true;
 buildTimeTooltipTick();
 if(nativeCalls!=1||!nativeWanted||!buildTimeTooltipClaimed||!buildTimeTooltipActive){puts("enable regression");return 2;}
 unsigned measured=nativeCalls;
 for(unsigned i=0;i<30;++i){now+=8;buildTimeTooltipTick();}
 if(nativeCalls!=measured){puts("250ms throttle regression");return 3;}
 now+=260;buildTimeTooltipTick();
 if(nativeCalls!=measured+1){puts("active owner recheck regression");return 4;}
 instance.buildTimeTooltip=false;buildTimeTooltipTick();
 if(nativeCalls!=measured+2||nativeWanted||buildTimeTooltipClaimed||buildTimeTooltipActive){puts("restore regression");return 5;}
 measured=nativeCalls;
 for(unsigned i=0;i<1000;++i){now+=8;buildTimeTooltipTick();}
 if(nativeCalls!=measured){puts("repeated disabled native owner call");return 6;}
 instance.buildTimeTooltip=true;nativeFailure=1;buildTimeTooltipTick();
 if(nativeCalls!=measured+1||!buildTimeTooltipClaimed||buildTimeTooltipActive){puts("failed install claim lost");return 7;}
 instance.buildTimeTooltip=false;buildTimeTooltipTick();
 if(nativeCalls!=measured+2||nativeWanted||buildTimeTooltipClaimed){puts("failed install cleanup regression");return 8;}
 instance.buildTimeTooltip=true;nativeFailure=0;buildTimeTooltipTick();
 if(!buildTimeTooltipActive||!buildTimeTooltipClaimed){puts("reenable regression");return 9;}
 unsigned oldNative=nativeCalls;
 ++debugentries::policyRevision;buildTimeTooltipTick();
 if(nativeCalls!=oldNative+2||!buildTimeTooltipActive||!buildTimeTooltipClaimed){puts("policy change regression");return 10;}
 instance.buildTimeTooltip=false;buildTimeTooltipTick();
 if(buildTimeTooltipActive||buildTimeTooltipClaimed){puts("final cleanup regression");return 11;}
 std::printf("PASS build-time hot idle: zero native calls on 2000 disabled ticks, nonsticky failures, 250ms owner check, policy rebind (%u native calls)\n",nativeCalls);
}
"""
with tempfile.TemporaryDirectory(prefix="ra2-tooltip-idle-") as tmp:
    target=Path(tmp)
    (target/"tick.cpp").write_text(code)
    compiler=target/"tick"
    subprocess.run(["g++","-std=c++17","-O2","-Wall","-Wextra",
                    "-Werror","-UNDEBUG",str(target/"tick.cpp"),"-o",str(compiler)],check=True)
    subprocess.run([str(compiler)],check=True)
