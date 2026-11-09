#!/usr/bin/env python3
"""Compile actual Runtime/Host superweapon functions with explicit native/UI seams.

Exercises superSelect, namedSuper, cancelSuperSidebarSelection and the COMPLETE
Host result() function, without replacing their branch bodies. The memory map,
native calls, queue admission and Win32 notification boundary are controlled.
This is not a Windows, game, network or native-superweapon execution test.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def function(text, signature):
    """Return a C++ function including its balanced body; ignore comments/literals."""
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 0
    state = "code"
    i = opening
    while i < len(text):
        c = text[i]
        nxt = text[i:i + 2]
        if state == "line":
            if c == "\n":
                state = "code"
        elif state == "block":
            if nxt == "*/":
                state = "code"
                i += 1
        elif state in ('"', "'"):
            if c == "\\":
                i += 1
            elif c == state:
                state = "code"
        elif nxt == "//":
            state = "line"
            i += 1
        elif nxt == "/*":
            state = "block"
            i += 1
        elif c in ('"', "'"):
            state = c
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
        i += 1
    raise ValueError("unterminated function: " + signature)


PREAMBLE = r'''
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <type_traits>
#include <vector>
using DWORD=uint32_t;
enum Command {SelectSuper=1,SelectIron,SelectChrono,AttackOwnGround,
 PlaceCompletedBuilding,OpenTech,DestructPlayer,AirPathMove,StickyGuard,
 ProduceReserved,ProduceDeferred,SelectType,RevealMap,AutoBoard,Produce,
 Repair,Cheer,Taunt,ClearBeacons,SwitchPlayer,RestorePlayer,
 CancelTechProduction,SmartUnload,CancelAdvanced};
struct Shared {uint32_t ack=1,request=1;int result=0,count=0,command=SelectSuper,
 arg=0,arg2=0;uint32_t requestToken=0;uint64_t requestTypeHash=0;
 int techParameterStatus=0;bool stop=false;struct{bool active=false;}placementPreview;};
static Shared storage;static Shared*shared=&storage,*sh=&storage;
struct Snapshot {int status=1,session=0,typeCount=0;uint32_t match=1;
 bool chat=false;int types[1]{};int loadUnits=0,loadVehicles=0,loadWaiting=0,
 loadUnassigned=0;}snap;
static uint32_t ack=0,commandLatency=0,sentAt=0,match=1,inFlightMatch=1,
 autoRevealMatch=0,autoRevealDone=0,autoRevealRetry=0,pid=1;
static unsigned inFlightRetries=0;static uint64_t techAcknowledgedHash=0;
static bool fault=false,missileCleanup=false,techEnabled=false;
static int game=1,ui=1;static std::wstring destructPendingTarget=L"selected";
static constexpr auto DestructForeignNotice=L"foreign";
static struct{int mapStrategy=0;}extra;
static struct {bool has(int)const{return false;}void clear(){}}commandQueue,pointerMatches;
static std::map<uint32_t,uint32_t> memory;
static std::map<uint32_t,std::string> names;
static std::vector<std::wstring> notices;
static unsigned emitted=0,cursorCalls=0,dataWrites=0,checks=0,failures=0;
static bool ready=true,dependencies=true,queueOK=true,frameOK=true;
static constexpr uint32_t House=0x100000,Instance=0x200000,Type=0x300000;
template<class T>static T get(uint32_t address){return T(memory[address]);}
template<class T>static void put(uint32_t address,T value){memory[address]=uint32_t(value);++dataWrites;}
struct Vec{int count=1;};static Vec vec(uint32_t,unsigned){return {};}
static uint32_t item(Vec,int index){return index==0?Instance:0;}
static bool sameid(const char*address,const char*name){return names[uint32_t(uintptr_t(address))]==name;}
static bool nativeDependencyReady(uint32_t){return dependencies;}
template<class...T>static bool nativeDependenciesReady(T...){return dependencies;}
template<class R,class...T>static R call(uint32_t address,T...){
 if constexpr(std::is_void_v<R>){++cursorCalls;return;}
 else return R(address==0x6CC360&&ready);
}
static void unselect(){++cursorCalls;}
static DWORD sampleClock(Shared*){return 100;}
namespace orderpolicy{enum class Reject{None};static Reject superWeaponEvent(bool,bool,bool,int){return Reject::None;}}
namespace taskarbiter{enum class Owner{SuperWeapon};static uint32_t syntheticSuperWeaponActor(int n){return uint32_t(n+1);}}
namespace gateway {
 struct Context{uint32_t match,tick;bool ready,chat;};
 struct Intent{uint32_t match,tick,actor,type,token;};
 enum class Failure{None};static Failure validate(const Context&,const Intent&,unsigned){return Failure::None;}
 static int makeQueuedReceipt(unsigned,const Intent&,uint32_t,taskarbiter::Owner){return 0;}
}
static struct {void begin(uint32_t,int){}bool canIssue(uint32_t,int){return frameOK;}
 void record(uint32_t,int,taskarbiter::Owner){}}actorFrameArbiter;
static struct {unsigned nextSerial=1;}missionObserver;
namespace vanilla{struct Cell{short x,y;};}
namespace intent{struct Event{int player,type;vanilla::Cell cell;};static Event superWeapon(int player,int,int type,vanilla::Cell cell){return {player,type,cell};}}
static intent::Event lastEvent{};
static bool submitConstructed(const intent::Event&e){if(!queueOK)return false;++emitted;lastEvent=e;return true;}
static void recordReceipt(int){}
static unsigned rejected=0;
// Production snap.commandRejected is represented by a reference member below.
struct FullSnapshot:Snapshot{unsigned&commandRejected=rejected;};
#define snap fullSnapshot
static FullSnapshot fullSnapshot;
static void MemoryBarrier(){}static DWORD GetTickCount(){return 100;}
static void diag(const std::wstring&){}static void critical(const std::wstring&){}
static void notify(const std::wstring&message){notices.push_back(message);}
static int GetDlgItem(int,int){return 0;}
static void setConsoleFeedback(int,const std::wstring&){}static void setConsoleRuntimeError(int,bool){}
static void updatePlacementEventUI(){}static void logBridgeHealth(bool){}
static std::wstring submitDiagnostic(){return L"controlled queue";}
static void airPathResult(int,int,uint32_t,uint32_t){}
static void constructionResult(int,int,uint32_t,uint32_t){}
namespace selectedmode{static int index(int){return -1;}static const wchar_t*label(int){return L"selected";}}
namespace delivery{template<class...T>static bool retrySelection(T...){return false;}}
namespace eventdelivery{static bool isMultiplayer(int){return false;}}
namespace buildingmodes{constexpr int TechParameters=4,TechData=3;}
static bool typeIndexedCommand(int,int){return false;}
static uint64_t typeListHash(const int*,int){return 1;}
static bool foregroundGame(){return true;}
template<class...T>static bool queueCommand(T...){return false;}
static void check(bool condition,const char*name){++checks;if(!condition){++failures;std::cerr<<"FAIL "<<name<<"\n";}}
static bool said(const wchar_t*part){return !notices.empty()&&notices.back().find(part)!=std::wstring::npos;}
static void reset(bool targeted=true){
 storage={};shared=sh=&storage;ack=0;notices.clear();memory.clear();names.clear();
 emitted=cursorCalls=dataWrites=0;ready=dependencies=queueOK=frameOK=true;fault=false;
 memory[Instance+0x2C]=House;memory[Instance+0x6D]=1;memory[Instance+0x28]=Type;
 memory[Type+0xBC]=targeted?1:0;memory[Type+0x98]=7;memory[House+0x30]=2;memory[0xA8ED84]=90;
 names[Type+0x24]="IronCurtainSpecial";
}
'''

CASES = r'''
static void receipt(int value,int command=SelectSuper,int argument=0){
 storage.command=command;storage.arg=argument;storage.result=value;storage.count=value>0;
 result();
}
int main(){
 reset();int r=superSelect(House,0);receipt(r);
 check(r==1&&emitted==0&&cursorCalls>0&&memory[0x8809A0]==0,"targeted native cursor unchanged");
 check(said(L"选择目标")&&!said(L"入队"),"targeted receipt asks for a target");
 reset(false);r=superSelect(House,0);receipt(r);
 check(emitted==1&&cursorCalls==0&&dataWrites==0&&lastEvent.player==2&&lastEvent.type==7&&lastEvent.cell.x==0&&lastEvent.cell.y==0,"instant action queues exactly one native order without cursor edits");
 check(r==2,"instant queued result is distinguishable from target selection");
 check(said(L"入队")&&!said(L"选择目标"),"instant receipt says queued and does not ask for a target");
 reset(false);queueOK=false;r=superSelect(House,0);receipt(r);
 check(r==-6&&emitted==0&&dataWrites==0&&said(L"未能提交"),"failed queue has no success feedback");
 reset();ready=false;r=superSelect(House,0);receipt(r);
 check(r==-5&&emitted==0&&dataWrites==0&&said(L"尚未就绪"),"unready native superweapon remains rejected");
 reset();dependencies=false;r=superSelect(House,0);receipt(r);
 check(r==-12&&emitted==0&&dataWrites==0&&said(L"不支持"),"missing native dependency remains rejected");
 reset();memory[0x8809A0]=4;r=cancelSuperSidebarSelection();receipt(r,SelectSuper,-1);
 check(r==1&&emitted==0&&memory[0x8809A0]==uint32_t(-1)&&said(L"已取消")&&!said(L"请在地图"),"right-click cancel already has dedicated feedback");
 reset();memory[0x8809A0]=uint32_t(-1);r=cancelSuperSidebarSelection();receipt(r,SelectSuper,-1);
 check(r==0&&emitted==0&&cursorCalls==0&&notices.empty(),"idle cancel remains a silent no-op");
 reset();r=namedSuper(House,SelectIron);receipt(r,SelectIron);
 check(r==1&&emitted==0&&said(L"选择目标"),"named iron hotkey retains targeted behavior");
 reset();names[Type+0x24]="ChronoSphereSpecial";r=namedSuper(House,SelectChrono);receipt(r,SelectChrono);
 check(r==1&&emitted==0&&said(L"选择目标"),"named chrono hotkey retains targeted behavior");
 reset(false);frameOK=false;r=superSelect(House,0);receipt(r);
 check(r==-6&&emitted==0&&said(L"未能提交"),"same-frame actor rejection never reports queued");
 std::cout<<"{\"checks\":"<<checks<<",\"failures\":"<<failures<<",\"passed\":"<<(failures?"false":"true")<<"}\n";
 return failures?1:0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--bridge", type=Path)
    parser.add_argument("--host", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    files = {
        "bridge": args.bridge or args.source_root / "source/assist/source/bridge.cpp",
        "host": args.host or args.source_root / "source/assist/source/main.cpp",
        "cancel": args.source_root / "source/assist/source/superweapon_sidebar_bridge.h",
        "test": Path(__file__).resolve(),
    }
    captured = {name: path.read_bytes() for name, path in files.items()}
    source = {name: data.decode("utf-8") for name, data in captured.items()}
    bodies = [function(source["bridge"], "static int superSelect("),
              function(source["bridge"], "static int namedSuper("),
              function(source["cancel"], "static int cancelSuperSidebarSelection("),
              function(source["host"], "static void result(){")]
    args.out.mkdir(parents=True, exist_ok=True)
    inputs = args.out / "inputs"
    inputs.mkdir(exist_ok=True)
    for name, data in captured.items():
        (inputs / (name + ".source")).write_bytes(data)
    cpp, binary = args.out / "fixture.cpp", args.out / "fixture"
    cpp.write_text(PREAMBLE + "\n" + "\n".join(bodies) + "\n" + CASES, encoding="utf-8")
    compile_command = ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", str(cpp), "-o", str(binary)]
    compiled = subprocess.run(compile_command, text=True, capture_output=True)
    (args.out / "compile.log").write_text(compiled.stdout + compiled.stderr)
    run = subprocess.run([str(binary)], text=True, capture_output=True) if not compiled.returncode else None
    if run:
        (args.out / "run.log").write_text(run.stdout + run.stderr)
    receipt = {
        "test": "actual superSelect / namedSuper / cancelSuperSidebarSelection / full result() feedback",
        "compile_command": compile_command, "compile_returncode": compiled.returncode,
        "run_command": [str(binary)], "run_returncode": run.returncode if run else None,
        "source_sha256": {name: {"path": str(path), "sha256": hashlib.sha256(captured[name]).hexdigest()} for name, path in files.items()},
        "function_sha256": [hashlib.sha256(body.encode()).hexdigest() for body in bodies],
        "scope": "Linux compiled production-function fixture; native queries, memory, queue admission, UI and mailbox handoff are explicit seams",
        "windows_executed": False, "game_executed": False, "network_executed": False,
        "result": json.loads(run.stdout) if run and run.stdout.startswith("{") else None,
    }
    (args.out / "receipt.json").write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(receipt, ensure_ascii=False))
    raise SystemExit(compiled.returncode or (run.returncode if run else 1))


if __name__ == "__main__":
    main()
