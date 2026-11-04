#!/usr/bin/env python3
"""Exercise the production reveal admission and CRC callback after failed cleanup.

The fixture uses synthetic memory and the real restoration model. No game is
attached, and this test does not establish multiplayer compatibility.
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
    tests = Path(__file__).resolve().parent
    bridge = (tests.parent / "source/crc_shadow_bridge.h").read_text(encoding="utf-8")
    callback = function(bridge, 'extern "C" unsigned __cdecl AssistMapCrcShadow(')
    admission = function(bridge, "static bool crcMapToolBegin(")
    recovery = (tests / "crc_shadow_recovery.cpp").read_text(encoding="utf-8")
    recovery = recovery[:recovery.index("int main()")].replace(
        '../source/crc_shadow_restore.h',
        (tests.parent / "source/crc_shadow_restore.h").as_posix())
    code = r'''
#include <cstdio>
RECOVERY
namespace crcshadow=assist::crcshadow;
static Fixture fixture;
static auto&crcMapModel=fixture.model;
struct Options {unsigned crcMapShadow=0;} options;
static Options*sh=&options;
static uint32_t match=7,crcMapEpoch=7,crcMapUnsafeEpoch=0,crcMapLocal=0x21000;
static bool crcMapActive=false,crcMapInstalled=false,crcMapFaulted=false;
static bool crcMapToolScope=false,crcMapNativeReconstructed=false,worldCurrent=true;
static int crcMapLastFrame=10,crcMapState=0;
static unsigned crcMapSubstitutions=0,crcMapFaults=0;
template<class T>static bool read(uint32_t at,T*out,size_t n){return fixture.memory.read(at,out,n);}
template<class T>static T get(uint32_t at){if(at==0xA8ED84)return T(11);T out{};read(at,&out,sizeof out);return out;}
static bool crcMapWorld(){return worldCurrent;}
static bool crcMapIdentity(uint32_t house,crcshadow::Identity&id){if(house!=fixture.id.pointer)return false;id=fixture.id;return true;}
static bool crcMapPointerTracked(uint32_t p){return p==fixture.id.pointer;}
static void crcMapUnknown(){crcMapFaulted=true;crcMapUnsafeEpoch=crcMapEpoch;crcMapState=-3;++crcMapFaults;}
static void crcMapPublish(){}
CALLBACK
ADMISSION
static void resetFixture(){
 fixture=Fixture{};options={};sh=&options;match=crcMapEpoch=7;crcMapUnsafeEpoch=0;
 crcMapActive=crcMapInstalled=crcMapFaulted=crcMapToolScope=crcMapNativeReconstructed=false;
 worldCurrent=true;crcMapLastFrame=10;crcMapState=0;crcMapSubstitutions=crcMapFaults=0;
}
int main(){
 int checks=0,failures=0;
 auto check=[&](bool ok,const char*what){++checks;if(!ok){++failures;std::fprintf(stderr,"FAIL: %s\n",what);}};
 resetFixture();
 check(crcMapToolBegin(fixture.id.pointer),"ordinary CRC-off reveal remains available");
 check(crcMapUnsafeEpoch==match&&!crcMapToolScope,"ordinary reveal does not activate CRC hooks");
 // The real restoration helper retains ownership on a failed data write.
 resetFixture();crcMapActive=crcMapInstalled=true;fixture.memory.mode=Memory::FailBefore;
 const auto failed=fixture.restore();
 check(!failed.complete&&failed.retained==1,"failed restoration retains owned byte");
 check(fixture.model.find(fixture.id)->original==0,"original byte exists before second reveal");
 const bool admitted=crcMapToolBegin(fixture.id.pointer);
 if(admitted){AssistMapCrcShadow(6,fixture.id.pointer,1);fixture.memory.byte=1;}
 check(!admitted,"CRC-off must not admit reveal while restoration retains hooks");
 check(fixture.model.find(fixture.id)->original==0,"tool write cannot become a natural write");
 check(fixture.model.crcValue(fixture.id,1)==0,"pending CRC original survives rejected reveal");
 fixture.memory.mode=Memory::Success;
 check(fixture.restore().complete&&fixture.memory.byte==0,"retained data still restores");
 crcMapActive=crcMapInstalled=false;crcMapModel.clear();
 check(crcMapToolBegin(fixture.id.pointer),"reveal resumes after complete cleanup");
 // Code restoration can also fail after data restoration has completed.
 resetFixture();crcMapInstalled=true;
 check(!crcMapToolBegin(fixture.id.pointer),"retained code hook blocks untagged reveal");
 resetFixture();crcMapToolScope=true;
 check(!crcMapToolBegin(fixture.id.pointer),"retained tool scope blocks untagged reveal");
 resetFixture();crcMapActive=true;
 check(!crcMapToolBegin(fixture.id.pointer),"active mirror blocks untagged reveal");
 // The explicit CRC-on path still tags the write and retains its old value.
 resetFixture();options.crcMapShadow=1;crcMapActive=crcMapInstalled=true;
 check(crcMapToolBegin(fixture.id.pointer)&&crcMapToolScope,"CRC-on reveal enters tool scope");
 AssistMapCrcShadow(6,fixture.id.pointer,1);
 check(!crcMapFaulted&&fixture.model.find(fixture.id)->original==0,"tagged repeat retains original");
 crcMapToolScope=false;
 check(AssistMapCrcShadow(0,fixture.id.pointer,1)==0,"CRC-on read still uses owned original");
 resetFixture();options.crcMapShadow=1;
 check(!crcMapToolBegin(fixture.id.pointer),"requested but inactive CRC still rejects reveal");
 std::printf("CRC reveal transition: %d checks, %d failures\n",checks,failures);
 return failures?1:0;
}
'''.replace("RECOVERY", recovery).replace("CALLBACK", callback).replace("ADMISSION", admission)
    args.build_dir.mkdir(parents=True, exist_ok=True)
    cpp = args.build_dir / "crc_shadow_reveal_transition.cpp"
    exe = args.build_dir / "crc_shadow_reveal_transition.exe"
    cpp.write_text(code, encoding="utf-8")
    subprocess.run([str(args.zig.resolve()), "c++", "-std=c++20", "-O0", str(cpp.resolve()), "-o", str(exe.resolve())], check=True)
    subprocess.run([str(exe.resolve())], check=True)


if __name__ == "__main__":
    main()
