#!/usr/bin/env python3
"""Production pause key service through the complete Runtime message filter.

Actual Registry/Tracker, pause availability/observe/restore/tick functions,
recordRuntimeGesture, AssistFilterMessage and AssistRuntimeFilter are compiled.
Win32 delivery, native memory/patches and unrelated inactive features are seams.
The fixture omits a release delivered outside the game, performs real pause
maintenance for focus loss/rebinding/disable, then delivers a new physical key
pair through the actual outer Runtime filter. No Windows/game run is claimed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from superweapon_result_feedback import function


PREAMBLE = r'''
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <type_traits>
#include "hotkey_routes.h"
#include "pause_view_policy.h"
#include "pointer_intent.h"
#include "read_only_lifecycle.h"
using namespace assist;
#define __cdecl
using DWORD=uint32_t;using WPARAM=uintptr_t;using LPARAM=intptr_t;
using HWND=uintptr_t;
struct MSG{HWND hwnd=1;unsigned message=0;WPARAM wParam=0;LPARAM lParam=0;};
struct RECT{int left=0,top=0,right=800,bottom=600;};
constexpr unsigned WM_NULL=0,WM_KEYDOWN=0x100,WM_KEYUP=0x101,WM_SYSKEYDOWN=0x104,WM_SYSKEYUP=0x105,WM_MOUSEMOVE=0x200;
constexpr unsigned WM_LBUTTONDOWN=0x201,WM_LBUTTONUP=0x202,WM_LBUTTONDBLCLK=0x203,WM_RBUTTONDOWN=0x204,WM_RBUTTONUP=0x205,WM_RBUTTONDBLCLK=0x206;
constexpr unsigned WM_MBUTTONDOWN=0x207,WM_MBUTTONUP=0x208,WM_MBUTTONDBLCLK=0x209,WM_XBUTTONDOWN=0x20b,WM_XBUTTONUP=0x20c,WM_XBUTTONDBLCLK=0x20d;
constexpr unsigned VK_LBUTTON=1,VK_RBUTTON=2,VK_MBUTTON=4,VK_XBUTTON1=5,VK_XBUTTON2=6,XBUTTON1=1,XBUTTON2=2;
static unsigned HIWORD(uintptr_t value){return unsigned(value>>16)&65535;}
static unsigned LOWORD(uintptr_t value){return unsigned(value)&65535;}
static uint8_t physicalKeys[256]{};
static short GetKeyState(unsigned key){return physicalKeys[key]?-32768:0;}
static uint32_t clockNow=100,match=1,lastHouse=0x100000;
static bool fault=false;static keygesture::Tracker runtimeKeys;
static std::map<uint32_t,uint32_t> memory;
template<class T>static T get(uint32_t at){return T(memory[at]);}
static bool read(uint32_t,void*out,size_t bytes){std::memset(out,0,bytes);return true;}
struct Shared {bool stop=false,foreground=true;uint32_t moFeatures=16,heartbeat=100;
 uint32_t readOnly=0,readOnlyRequest=0,readOnlyAck=0;
 uint32_t pauseViewKeys[2]{0x74,0x75};int pauseViewStatus=0;uint32_t pointerPolicySeq=0,flags=0;
 pointerinput::NativePolicy pointerPolicy{};pointerinput::Mailbox pointer{};
 pointerinput::SuppressedClick suppressedClick{};};
static Shared storage;static Shared*sh=&storage;
static readonly::RuntimeState readOnlyState;
static uint32_t sampleClock(Shared*){return clockNow;}
namespace experimentalpatch{constexpr uint32_t PauseLoop=0x55D360;}
struct RiskSite{uint32_t address;unsigned size;uint8_t before[16],after[16];};
static bool restoreRiskSite(RiskSite&){return true;}
static bool installRiskSite(RiskSite&){return true;}
static void pauseViewLoopEntry(){}
static bool nativePlacementCompletionPending(){return false;}
static void nativePlacementCompletionTick(uint32_t){}
static bool nativePlacementMessage(MSG*){return false;}
static bool moOrdersMessage(MSG*){return false;}
static bool formationMessage(MSG*){return false;}
static bool reservationMessage(MSG*){return false;}
static bool nativeReady(){return true;}
static unsigned GetCurrentProcessId(){return 1;}
static HWND GetForegroundWindow(){return 1;}
static unsigned GetWindowThreadProcessId(HWND,DWORD*p){*p=1;return 1;}
static void MemoryBarrier(){}
static bool cooperativeSingleInput(uint32_t){return false;}
static int outgoingRoom(){return 1;}
static bool GetClientRect(HWND,RECT*){return true;}
static uint32_t pointerCell=0,pointerTarget=0,pointerTargetID=0;static int pointerGroundZ=0;
static bool pointerLocation(pointerinput::Intent&){return false;}
static bool nativeEnemy(uint32_t,uint32_t){return false;}
struct Vec{uint32_t items=0;int count=0;};static Vec vec(uint32_t){return {};}
static uint32_t item(Vec,int){return 0;}
static struct{bool contains(uint32_t){return false;}}worldObjects;
static bool own(uint32_t,uint32_t){return false;}
static int kind(uint32_t){return 0;}
static bool rawArmed(uint32_t){return false;}
static bool rawMagnetron(uint32_t){return false;}
static bool csAllowedFor(uint32_t,bool){return false;}
namespace featuretypes{constexpr int AdvancedAttack=0;}
static bool featureObjectAllowed(int,uint32_t){return false;}
constexpr uint32_t AdvancedInput=1,MagnetronInput=2;
static uint32_t blockedButtons=0;static pointerinput::SuppressedClick blockedOrigins[2];
namespace gameentry{enum class Client{Assist};template<class F>static bool scoped(Client,bool,F f){f();return true;}}
static bool superweaponGrantWantsInputSample(const MSG*){return false;}
static bool nativePlacementWantsInputSample(const MSG*){return false;}
namespace dispatchrouter{enum class Trigger{Message};}
static long hostWorkPending=0;static long InterlockedExchange(long*p,long value){auto old=*p;*p=value;return old;}
static DWORD GetTickCount(){return clockNow;}
static void dispatchFromTrigger(dispatchrouter::Trigger,DWORD,const MSG*){}
namespace runtime{constexpr unsigned Tactical=1;}
static struct{unsigned enabled=runtime::Tactical;}controlStorage;
static auto*control=&controlStorage;
extern "C" int AssistFilterMessage(void*);
extern "C" int GuardedAssistFilter(void* message){return AssistFilterMessage(message);}
'''

CASES = r'''
static unsigned checks=0,failures=0;
static void check(bool value,const char*name){++checks;if(!value){++failures;std::cerr<<"FAIL "<<name<<"\n";}}
static void reset(){
 storage={};sh=&storage;memory.clear();std::memset(physicalKeys,0,sizeof physicalKeys);
 readOnlyState={};
 runtimeKeys.clear();pauseViewState={};pauseViewHotkeys.clear();pauseViewKeyEpoch=0;
 std::memset(pauseViewKeySuppressed,0,sizeof pauseViewKeySuppressed);
 pauseViewInstalled=pauseViewNativeFailed=false;clockNow=100;match=1;fault=false;
 memory[0xA8B238]=5;memory[0xA8E9A0]=1;memory[0xA8ED80]=1;
 memory[0xA83D4C]=lastHouse;memory[0xA8B230]=0x200000;memory[0xA8ED84]=100;
 pauseViewTick();
}
static unsigned deliver(unsigned type,unsigned key=0,bool repeat=false){
 ++clockNow;sh->heartbeat=clockNow;
 if(type==WM_KEYDOWN||type==WM_SYSKEYDOWN)physicalKeys[key]=1;
 if(type==WM_KEYUP||type==WM_SYSKEYUP)physicalKeys[key]=0;
 MSG msg{1,type,key,repeat?LPARAM(1u<<30):0};deliverRuntimeMessage(&msg);return msg.message;
}
static void loseRelease(){
 check(deliver(WM_KEYDOWN,0x74)==WM_NULL,"initial configured press is owned");
 sh->foreground=false;pauseViewTick();
 // Mouse movement over the unfocused game is still on its existing message
 // hook allowlist. It observes false foreground and clears the actual Tracker.
 deliver(WM_MOUSEMOVE);physicalKeys[0x74]=0; // release went to another window
 check(runtimeKeys.held[0x74]==0&&!pauseViewState.paused,"focus-loss maintenance clears held gesture and pause");
}
static void ordinaryPair(const char*name){
 const bool down=deliver(WM_KEYDOWN,0x74)==WM_KEYDOWN;
 const bool up=deliver(WM_KEYUP,0x74)==WM_KEYUP;
 check(down&&up,name);
}
int main(){
 reset();loseRelease();sh->pauseViewKeys[0]=0x76;pauseViewTick();sh->foreground=true;pauseViewTick();
 ordinaryPair("rebind after outside release preserves both edges of the new ordinary key");
 check(deliver(WM_KEYDOWN,0x76)==WM_NULL&&deliver(WM_KEYUP,0x76)==WM_NULL,"replacement pause binding still owns its pair");
 reset();loseRelease();sh->moFeatures=0;pauseViewTick();sh->foreground=true;pauseViewTick();
 check(!pauseViewInstalled,"explicit disable runs real restoration");
 ordinaryPair("disable after outside release preserves both edges of the new ordinary key");
 reset();loseRelease();sh->pauseViewKeys[0]=0x274;pauseViewTick();sh->foreground=true;pauseViewTick();
 ordinaryPair("modifier rebinding does not swallow plain-key release");
 reset();check(deliver(WM_KEYDOWN,0x74)==WM_NULL,"normal owned down");
 const bool paused=pauseViewState.paused;deliver(WM_KEYDOWN,0x74,true);
 check(pauseViewState.paused==paused,"autorepeat does not retrigger pause");
 check(deliver(WM_KEYUP,0x74)==WM_NULL,"autorepeat retains original owned release");
 reset();check(deliver(WM_KEYDOWN,0x74)==WM_NULL,"owned press before same-hold disable");
 sh->moFeatures=0;pauseViewTick();
 check(deliver(WM_KEYUP,0x74)==WM_NULL,"disable without a new press still consumes its owned release");
 ordinaryPair("later ordinary pair remains untouched");
 reset();check(deliver(WM_SYSKEYDOWN,0x74)==WM_NULL,"system-key down uses same ownership");
 sh->foreground=false;pauseViewTick();deliver(WM_MOUSEMOVE);physicalKeys[0x74]=0;
 sh->moFeatures=0;pauseViewTick();sh->foreground=true;
 check(deliver(WM_SYSKEYDOWN,0x74)==WM_SYSKEYDOWN&&deliver(WM_SYSKEYUP,0x74)==WM_SYSKEYUP,"new system-key pair drops lost-release ownership");
 reset();loseRelease();sh->foreground=true;pauseViewTick();
 check(deliver(WM_KEYDOWN,0x74)==WM_NULL&&deliver(WM_KEYUP,0x74)==WM_NULL,"unchanged active binding reclaims the fresh pair");
 for(int gate=0;gate<3;++gate){
  reset();if(gate==0)sh->readOnly=1;if(gate==1)sh->readOnlyRequest=1;if(gate==2)readOnlyState.active=true;
  check(deliver(WM_KEYDOWN,0x74)==WM_KEYDOWN&&deliver(WM_KEYUP,0x74)==WM_KEYUP,
        "read-only flag, pending retirement or active retirement keeps pause key pair native");
  check(!pauseViewState.paused&&!runtimeKeys.held[0x74],"read-only input gate does not mutate pause or record held input");
 }
 for(int gate=0;gate<3;++gate){
  reset();check(deliver(WM_KEYDOWN,0x74)==WM_NULL,"owned pause down before read-only transition");
  const bool paused=pauseViewState.paused;
  if(gate==0)sh->readOnly=1;if(gate==1)sh->readOnlyRequest=1;if(gate==2)readOnlyState.active=true;
  deliver(WM_MOUSEMOVE);
  check(deliver(WM_KEYUP,0x74)==WM_NULL&&!pauseViewKeySuppressed[0x74],
        "read-only transition balances original pause release after an unrelated message");
  check(deliver(WM_KEYDOWN,0x74)==WM_KEYDOWN&&deliver(WM_KEYUP,0x74)==WM_KEYUP,
        "a fresh pause-key pair during read-only remains native");
  check(pauseViewState.paused==paused&&!runtimeKeys.held[0x74],
        "retiring owned release neither toggles pause nor samples keys");
  reset();deliver(WM_KEYDOWN,0x74);
  if(gate==0)sh->readOnly=1;if(gate==1)sh->readOnlyRequest=1;if(gate==2)readOnlyState.active=true;
  physicalKeys[0x74]=0;
  check(deliver(WM_KEYDOWN,0x74)==WM_KEYDOWN&&deliver(WM_KEYUP,0x74)==WM_KEYUP,
        "a new read-only key down retires an older release lost outside the input route");
 }
 reset();deliver(WM_SYSKEYDOWN,0x74);sh->readOnly=1;
 check(deliver(WM_SYSKEYUP,0x74)==WM_NULL,"read-only system-key release balances its owned down");
 check(deliver(WM_SYSKEYDOWN,0x74)==WM_SYSKEYDOWN&&deliver(WM_SYSKEYUP,0x74)==WM_SYSKEYUP,
       "fresh read-only system-key pair remains native");
 std::cout<<"{\"checks\":"<<checks<<",\"failures\":"<<failures<<",\"passed\":"<<(failures?"false":"true")<<"}\n";
 return failures?1:0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--pause", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    files = {
        "pause": args.pause or args.source_root / "source/assist/source/pause_view_bridge.h",
        "message": args.source_root / "source/assist/source/native_message_policy.h",
        "runtime": args.source_root / "source/game_runtime/source/runtime.cpp",
        "registry": args.source_root / "source/assist/source/hotkey_routes.h",
        "tracker": args.source_root / "source/shared/source/key_gesture.h",
        "policy": args.source_root / "source/assist/source/pause_view_policy.h",
        "pointer": args.source_root / "source/assist/source/pointer_intent.h",
        "readonly": args.source_root / "source/assist/source/read_only_lifecycle.h",
        "test": Path(__file__).resolve(),
        "extractor": Path(__file__).with_name("superweapon_result_feedback.py"),
    }
    captured = {key: path.read_bytes() for key, path in files.items()}
    text = {key: data.decode("utf-8") for key, data in captured.items()}
    pause = text["pause"][:text["pause"].index("// Each non-pumping native draw/input")]
    # Native functions/assembly are outside the input pairing scope. The tick
    # installs through a controlled patch boundary but retains its real gates.
    bodies = [pause, function(text["pause"], "static void pauseViewTick(){"),
              function(text["message"], "static void recordRuntimeGesture("),
              function(text["message"], 'extern "C" int AssistFilterMessage('),
              function(text["message"], 'extern "C" void AssistRuntimeFilter(')]
    # Retain the actual GameRuntimeHook message allowlist as well. Controller
    # liveness/lease ownership above this line are an explicit established-
    # connection seam; every replayed input still crosses this exact selector.
    allowed = [line.strip() for line in text["runtime"].splitlines()
               if "if((control->enabled&runtime::Tactical)" in line and "AssistRuntimeFilter((void*)lp);" in line]
    if len(allowed) != 1:
        raise ValueError("Runtime message allowlist changed; review the fixture")
    bodies.append("static void deliverRuntimeMessage(MSG*msg){auto lp=reinterpret_cast<LPARAM>(msg);" + allowed[0] + "}")
    args.out.mkdir(parents=True, exist_ok=True)
    inputs = args.out / "inputs"
    inputs.mkdir(exist_ok=True)
    for key, data in captured.items():
        (inputs / (key + ".source")).write_bytes(data)
    # Compile headers captured at the same boundary, including their original
    # relative paths, so a concurrent source edit cannot change the test input.
    frozen = args.out / "frozen/source"
    for key, relative in (("registry", "assist/source/hotkey_routes.h"),
                          ("tracker", "shared/source/key_gesture.h"),
                          ("policy", "assist/source/pause_view_policy.h"),
                          ("pointer", "assist/source/pointer_intent.h"),
                          ("readonly", "assist/source/read_only_lifecycle.h")):
        target = frozen / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(captured[key])
    cpp, binary = args.out / "fixture.cpp", args.out / "fixture"
    cpp.write_text(PREAMBLE + "\n" + "\n".join(bodies) + "\n" + CASES)
    command = ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-I" + str(frozen / "assist/source"), str(cpp), "-o", str(binary)]
    compiled = subprocess.run(command, text=True, capture_output=True)
    (args.out / "compile.log").write_text(compiled.stdout + compiled.stderr)
    run = subprocess.run([str(binary)], text=True, capture_output=True) if not compiled.returncode else None
    if run:
        (args.out / "run.log").write_text(run.stdout + run.stderr)
    receipt = {
        "test": "production pause pairing via AssistRuntimeFilter and AssistFilterMessage",
        "compile_command": command, "compile_returncode": compiled.returncode,
        "run_command": [str(binary)], "run_returncode": run.returncode if run else None,
        "source_sha256": {key: {"path": str(path), "sha256": hashlib.sha256(captured[key]).hexdigest()} for key, path in files.items()},
        "function_sha256": [hashlib.sha256(body.encode()).hexdigest() for body in bodies],
        "scope": "Linux controlled production input path; Win32 delivery, native memory/patch and unrelated inactive features are seams",
        "windows_executed": False, "game_executed": False,
        "result": json.loads(run.stdout) if run and run.stdout.startswith("{") else None,
    }
    (args.out / "receipt.json").write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(receipt, ensure_ascii=False))
    raise SystemExit(compiled.returncode or (run.returncode if run else 1))


if __name__ == "__main__":
    main()
