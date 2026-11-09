"""Exercise the production Host hotkey entry points across display failures.

The C++ fixture extracts the actual command identity, registration, dispatch,
WM_HOTKEY and sampler functions. Win32 calls, unrelated feature consumers and
the final command sink are controlled boundaries. It does not run Windows or
the game, and a recorded AutoBoard means Host dispatch, not game execution.

The negative control compiles frozen, SHA-256-checked pre.3 main.cpp and
hotkey_host.h bytes. --negative-control-ref selects that recorded provenance;
it never requires a local Git object. The current registry is deliberately
kept in both variants, isolating the display/context gate from message recovery.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


SCOPE = (
    "production Host identity, registration and hotkey dispatch functions with "
    "simulated Win32 and command sink; not real Windows or game execution"
)
BASELINE_COMMIT = "93f02787d42b61ff5c30e450317f99ba00673149"
BASELINE_FIXTURES = {
    "source/assist/tests/fixtures/hotkey_pre1473/main.cpp": "c824b03b81a7ee2cef824db5b04498dd4ca7c7935c956960ccc10ee0280b1b1f",
    "source/assist/tests/fixtures/hotkey_pre1473/hotkey_host.h": "5650d38558132a364500360848e45afd55953d3691c4b8cd581d13f01b34ad6c",
}


def function(text: str, name: str, *, optional: bool = False) -> str:
    pattern = rf"static\s+[^;{{\n]*\b{re.escape(name)}\s*\([^;{{]*\)\s*\{{"
    found = re.search(pattern, text)
    if not found:
        if optional:
            return ""
        raise ValueError(f"Production function definition missing: {name}")
    start = found.start()
    at = text.index("{", found.start())
    depth = 1
    end = at + 1
    while depth:
        if end >= len(text):
            raise ValueError(f"Unclosed production function: {name}")
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


PREAMBLE = r'''
#include "protocol.h"
#include "additional_settings.h"
#include "control_link.h"
#include "read_only_lifecycle.h"
#include "hotkey_routes.h"
#include "type_hotkey.h"
#include "view_zoom.h"
#include "activation_diagnostic.h"
#include "destruct_owner_input.h"
#include "placement_event_input.h"
#include "../../shared/source/feature_availability.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace assist;
using DWORD=uint32_t;using WORD=uint16_t;using BYTE=uint8_t;
using UINT=unsigned;using WPARAM=uintptr_t;using LPARAM=intptr_t;using HWND=uintptr_t;
constexpr UINT MOD_ALT=1,MOD_CONTROL=2,MOD_SHIFT=4,MOD_NOREPEAT=0x4000;
constexpr unsigned HOTKEYF_SHIFT=1,HOTKEYF_CONTROL=2,HOTKEYF_ALT=4;
constexpr int CB_SETCURSEL=1;
static unsigned LOBYTE(uintptr_t v){return v&255;}
static unsigned HIBYTE(uintptr_t v){return (v>>8)&255;}
static unsigned LOWORD(uintptr_t v){return v&65535;}
static unsigned HIWORD(uintptr_t v){return (v>>16)&65535;}
static WORD MAKEWORD(unsigned a,unsigned b){return WORD((a&255)|((b&255)<<8));}
static DWORD now=10000,lastData=10000,messageAt=10000;
static DWORD GetTickCount(){return now;}
static long GetMessageTime(){return static_cast<int32_t>(messageAt);}
static uint8_t rawKeys[256]{};
static short GetAsyncKeyState(int key){return rawKeys[key]?short(0x8000):0;}
static auto storage=std::make_unique<Shared>();
static Shared*shared=storage.get();static Snapshot snap{},incoming{};
static AdditionalSettings extra;
static bool enabled=true,focused=true,hasSelection=false,keysRegistered=false;
static bool hookSmartUnload=false,hookAutoCS=false,buildAnywhereMode=false;
static bool formationOwns=false,middleOwns=false,osInstallOkay=true;
static int registeredHotkeyMode=0,doubleMs=350;static HWND ui=1;
static bool foregroundGame(){return focused;}
static bool formationInputOwns(){return formationOwns;}
static bool runtimeMiddleOwnsNow(){return middleOwns;}
static bool formationHotkey(WORD){return false;}
static typekeys::Tap typeTap,variantTap,formationTap;
static hotkeyroute::Registry hotkeyRegistry;static hotkeyroute::Pending hotkeyPending;
static std::vector<int> registeredKeyIDs,osRegistered,commands;
static std::vector<std::wstring> notifications;
static WORD hotkeys[12]{},ejectKey=0,unloadKey=0,csKey=0,idleAircraftKey=0,socialKeys[9]{};
struct Binding{std::string id;WORD key;};
static std::vector<Binding> swBindings,typeBindings;
static struct{int size=0;}commandQueue;
struct ControlUpdate{};
static void SendDlgItemMessageW(HWND,int,int,int,int){}
static void changedSetting(){}static void syncAdditional(){}static void save(){}
static void notify(const std::wstring&s){notifications.push_back(s);}
static void critical(const std::wstring&){}
struct HotkeyDiagnostics{uint32_t messages=0,sampled=0,actions=0,blocked=0,fallbacks=0,lastError=0;};
static HotkeyDiagnostics hotkeyDiagnostics;
static void logHotkeyInput(const wchar_t*,int=0,uint16_t=0,DWORD=0){}
static void logHotkeyBindings(){}
static void traceHotkeyService(bool,bool,bool,bool){}
static void traceHotkeyKeys(const uint8_t*){}
static bool RegisterHotKey(HWND,int id,UINT,unsigned){if(!osInstallOkay)return false;osRegistered.push_back(id);return true;}
static bool UnregisterHotKey(HWND,int id){osRegistered.erase(std::remove(osRegistered.begin(),osRegistered.end(),id),osRegistered.end());return true;}
static DWORD GetLastError(){return 1409;}
static bool destructOwnerKeyConflict(WORD){return false;}
static unsigned destructSubmissions=0,placementSubmissions=0;
static void submitDestructSelectedOwner(){++destructSubmissions;}
static void submitPlacementEvent(){++placementSubmissions;}
static bool automaticHotkey(WPARAM){return false;}
static void selectNextIdleAircraft(){throw std::runtime_error("unexpected aircraft route");}
static void sendSelection(int,bool,const uint8_t* =nullptr){throw std::runtime_error("unexpected selection route");}
// This fixture's endpoint is the real Host dispatch into send(), not a game
// receipt. The separate command mailbox counter preserves batching behavior.
static void send(int command,int=0){commands.push_back(command);++shared->request;}
namespace watchhost {static void pin(uint32_t){}}
namespace external {namespace watch {
static bool firstBinding(const uint32_t*keys,int slot){
 if(slot<0||slot>=3||!(keys[slot]&255))return false;
 for(int n=0;n<slot;++n)if(keys[n]==keys[slot])return false;return true;
}
}}
static uint32_t displayMatch(){return snap.match;}
'''


TESTS = r'''
static unsigned checks=0,scenarios=0;
static void check(bool okay,const char*label){++checks;if(!okay)throw std::runtime_error(label);}
static void reset(int render=0){
 ++scenarios;now+=3000;lastData=messageAt=now;
 shared=storage.get();shared->stop=0;shared->request=shared->ack=0;
 shared->selectRequest=shared->selectAck=0;shared->formationPreview.active=0;
 incoming={};incoming.status=1;incoming.commandStatus=activation::Ready;incoming.match=8;
 snap=incoming;snap.renderStatus=render;snap.readStatus=render;
 enabled=focused=true;hasSelection=keysRegistered=false;registeredHotkeyMode=0;
 formationOwns=middleOwns=false;osInstallOkay=true;
 hookSmartUnload=hookAutoCS=buildAnywhereMode=false;
 extra={};extra.stickyKey=0;extra.variantKey=0;
 for(auto&key:extra.missionModeKeys)key=0;
 for(auto&key:extra.watchKeys)key=0;
 std::memset(hotkeys,0,sizeof hotkeys);hotkeys[11]=WORD('F'|0x200);
 std::memset(socialKeys,0,sizeof socialKeys);ejectKey=unloadKey=csKey=idleAircraftKey=0;
 std::memset(rawKeys,0,sizeof rawKeys);
 commandQueue.size=0;typeBindings.clear();swBindings.clear();notifications.clear();
 commands.clear();registeredKeyIDs.clear();osRegistered.clear();
 hotkeyRegistry.clear();hotkeyPending.clear();typeTap.clear();variantTap.clear();formationTap.clear();
 hotkeyDiagnostics={};destructSubmissions=placementSubmissions=0;
}
static void message(){
 ++now;messageAt=now;rawKeys[17]=rawKeys['F']=1;
 handleHotkey(1011,LPARAM(('F'<<16)|MOD_CONTROL));
}
static void acknowledge(){shared->ack=shared->request;shared->selectAck=shared->selectRequest;}
static void release(){++now;rawKeys['F']=0;serviceHotkeyBatch();rawKeys[17]=0;}
static void expectOneAutoBoard(const char*label){check(commands.size()==1&&commands[0]==AutoBoard,label);}

static void displayRegistration(){
 reset(0);
 check(commandChannelReady(),"fixture requires healthy command channel");
 check(desiredHotkeyMode()==2,"display failure must retain command hotkey mode");
 registerKeys();
 check(keysRegistered&&registeredHotkeyMode==2,"display failure must register command keys");
 check(hotkeyRegistry.find(1011)!=nullptr,"AutoBoard must retain its actual registration");
 check(std::find(osRegistered.begin(),osRegistered.end(),1011)!=osRegistered.end(),"AutoBoard must reach RegisterHotKey");
 message();expectOneAutoBoard("registered AutoBoard must dispatch with renderStatus zero");
 acknowledge();serviceHotkeyBatch();release();
 check(commands.size()==1,"sampler and release must not replay acknowledged AutoBoard");
}
static void displayMessage(){
 reset(1);registerKeys();snap.renderStatus=0;
 message();expectOneAutoBoard("WM_HOTKEY must dispatch AutoBoard after display failure");
}
static void displayService(){
 reset(1);registerKeys();const auto*group=hotkeyRegistry.find(1011);
 check(group!=nullptr,"fixture registration required before staged dispatch");
 check(hotkeyPending.add(*group,now,commandMatch()),"fixture must stage an authenticated hotkey action");
 snap.renderStatus=0;serviceHotkeyBatch();
 expectOneAutoBoard("pending hotkey must dispatch AutoBoard after display failure");
 check(!hotkeyPending.count,"dispatched AutoBoard must leave hotkey pending queue");
}
static void displayIdentityIndependence(){
 reset(0);snap.status=0;snap.readStatus=0;snap.chat=1;
 check(desiredHotkeyMode()==2,"authoritative command context must survive stale display metadata");
 registerKeys();message();expectOneAutoBoard("stale display chat/status must not override authenticated bridge context");
}
static void runtimeGates(){
 for(int state:{activation::Paused,activation::DisplayHidden,activation::ViewOnly,activation::AuthorityUnknown}){
  reset(1);registerKeys();incoming.commandStatus=state;
  check(desiredHotkeyMode()==0,"Runtime pause/menu/authority gate must unregister command keys");
  check(hotkeyPending.add(*hotkeyRegistry.find(1011),now,commandMatch()),"fixture must stage before Runtime gate");
  message();serviceHotkeyBatch();check(commands.empty()&&!hotkeyPending.count,"Runtime gate must discard both message and staged action");
 }
 reset(1);registerKeys();incoming.chat=1; // snap deliberately still says no chat
 check(desiredHotkeyMode()==0,"authoritative Runtime chat must close command context");
 message();serviceHotkeyBatch();check(commands.empty(),"Runtime chat must suppress AutoBoard");
 reset(1);registerKeys();focused=false;
 check(desiredHotkeyMode()==0,"focus loss must close command context");
 message();serviceHotkeyBatch();check(commands.empty(),"background hotkey must not dispatch");
 reset(1);registerKeys();enabled=false;
 check(desiredHotkeyMode()==0,"disabled module must close command context");
 message();serviceHotkeyBatch();check(commands.empty(),"disabled module must not dispatch");
 reset(1);registerKeys();shared->stop=1;
 check(desiredHotkeyMode()==0,"stopping bridge must close command context");
 message();serviceHotkeyBatch();check(commands.empty(),"stopping bridge must not dispatch");
 reset(1);registerKeys();incoming.status=-2;
 check(desiredHotkeyMode()==0,"faulted bridge must close command context");
 message();serviceHotkeyBatch();check(commands.empty(),"faulted bridge must not dispatch");
 reset(1);registerKeys();incoming.match=0;
 check(desiredHotkeyMode()==0,"unidentified bridge must close command context");
 message();serviceHotkeyBatch();check(commands.empty()&&!hotkeyPending.count,"unidentified bridge must not retain actions");
 reset(1);registerKeys();shared=nullptr;
 check(desiredHotkeyMode()==0,"missing mapping must close command context");
 message();serviceHotkeyBatch();check(commands.empty(),"missing mapping must not dispatch or dereference Shared");
}
static void staleConnectionRecovery(){
 reset(0);registerKeys();now+=1600;
 check(desiredHotkeyMode()==2,"transient freshness delay must preserve command registration identity");
 check(!commandFresh(now),"fixture command connection must be stale");
 check(hotkeyPending.add(*hotkeyRegistry.find(1011),now,commandMatch()),"fixture must stage action before stale service");
 message();serviceHotkeyBatch();check(commands.empty()&&!hotkeyPending.count,"stale connection must drop new and staged actions");
 lastData=now;serviceHotkeyBatch();check(commands.empty(),"connection recovery must not replay a held stale key");
 release();message();expectOneAutoBoard("new input after recovery must dispatch once");
}
static void inputOwnershipAndMailbox(){
 for(bool middle:{false,true}){
  reset(0);registerKeys();formationOwns=!middle;middleOwns=middle;
  check(hotkeyPending.add(*hotkeyRegistry.find(1011),now,commandMatch()),"fixture stage before input owner");
  serviceHotkeyBatch();check(commands.empty()&&!hotkeyPending.count,"formation/middle owner must still cancel generic actions");
 }
 reset(0);registerKeys();shared->selectRequest=1;
 message();check(commands.empty()&&hotkeyPending.count==1,"selection mailbox must gate the common command queue");
 shared->selectAck=1;serviceHotkeyBatch();expectOneAutoBoard("selection ACK must release exactly one AutoBoard");
}
static void polledHostTrigger(){
 for(bool installed:{true,false}){
  reset(0);osInstallOkay=installed;registerKeys();
  check(hotkeyRegistry.find(1011)!=nullptr,"failed OS registration must retain AutoBoard binding");
  check(osRegistered.empty()==!installed,"fixture must observe actual OS registration result");
  if(!installed)check(hotkeyDiagnostics.fallbacks==1&&hotkeyDiagnostics.lastError==1409,"actual registerKey failure must identify its polling fallback");
  ++now;rawKeys[17]=rawKeys['F']=1;serviceHotkeyBatch();
  expectOneAutoBoard("actual Host sampler must dispatch AutoBoard without WM_HOTKEY");
  acknowledge();serviceHotkeyBatch();serviceHotkeyBatch();
  check(commands.size()==1,"held polling fallback must not repeat AutoBoard");
  release();++now;rawKeys[17]=rawKeys['F']=1;serviceHotkeyBatch();
  check(commands.size()==2&&commands.back()==AutoBoard,"next physical press must dispatch another AutoBoard");
 }
}
static void sampledImmediateLifetime(){
 // Ordinary commands can use the next observed press after a busy Host.
 // Selection/cursor-sensitive one-shot actions keep their 150ms boundary.
 for(unsigned gap:{16u,150u,151u,501u})for(int action:{AttackOwnGround,DestructPlayer,PlaceCompletedBuilding}){
  reset(0);hotkeys[11]=0;
  if(action==AttackOwnGround)extra.missionModeKeys[selectedmode::OwnGround]=WORD('F'|0x200);
  else if(action==DestructPlayer){extra.destructSelectedOwner=true;extra.destructSelectedOwnerKey=WORD('F'|0x200);}
  else{buildAnywhereMode=true;extra.placementImplementation=buildingmodes::PlacementEvent;extra.placementEventKey=WORD('F'|0x200);}
  registerKeys();now+=gap;rawKeys[17]=rawKeys['F']=1;serviceHotkeyBatch();
  const unsigned sent=unsigned(commands.size())+destructSubmissions+placementSubmissions;
  check(sent==(gap<=150?1u:0u),"sample gap must preserve 150ms boundary for all three immediate actions");
  if(gap>150)check(!notifications.empty(),"expired sampled immediate action must explain rejection");
  else if(action==AttackOwnGround)check(commands==std::vector<int>{AttackOwnGround},"fresh sampled own-ground action must reach command sink");
  else if(action==DestructPlayer)check(destructSubmissions==1,"fresh sampled destruction must reach its immediate consumer once");
  else check(placementSubmissions==1,"fresh sampled placement must reach its immediate consumer once");
  acknowledge();serviceHotkeyBatch();check(unsigned(commands.size())+destructSubmissions+placementSubmissions==sent,"next sample must not revive a rejected or completed held immediate action");
 }
 reset(0);registerKeys();now+=501;rawKeys[17]=rawKeys['F']=1;serviceHotkeyBatch();
 expectOneAutoBoard("long sample gap must retain ordinary AutoBoard action");
}
static void readonlyMode(){
 reset(1);enabled=false;extra.watchSecondary=true;extra.watchKeys[0]=WORD('W'|0x600);
 check(desiredHotkeyMode()==1,"read-only secondary watch must retain its existing mode");
 registerKeys();check(hotkeyRegistry.find(1730)!=nullptr,"read-only watch registration must be preserved");
 check(hotkeyRegistry.find(1011)==nullptr,"read-only mode must not register AutoBoard");
 snap.renderStatus=0;check(desiredHotkeyMode()==0,"read-only display mode keeps its display gate");
}
int main(int argc,char**argv){
 try{
  const std::string which=argc>1?argv[1]:"all";
  if(which=="registration")displayRegistration();
  else if(which=="message")displayMessage();
  else if(which=="service")displayService();
  else if(which=="all"){
   displayRegistration();displayMessage();displayService();displayIdentityIndependence();
   runtimeGates();staleConnectionRecovery();inputOwnershipAndMailbox();polledHostTrigger();sampledImmediateLifetime();readonlyMode();
  }else throw std::runtime_error("unknown fixture selection");
  std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"scenarios\":"<<scenarios<<"}\n";return 0;
 }catch(const std::exception&e){
  std::cout<<"{\"passed\":false,\"checks\":"<<checks<<",\"scenarios\":"<<scenarios<<",\"failure\":\""<<e.what()<<"\"}\n";return 2;
 }
}
'''


def program(main: str, hotkeys: str) -> str:
    functions = []
    for name in (
        "commandLink", "commandMatch", "commandChannelReady", "commandFresh",
        "commandHotkeyContext", "desiredHotkeyMode", "modifiers",
        "synchronizeHotkeyKeys", "registerKey", "registerKeys",
    ):
        functions.append(function(main, name, optional=name == "commandHotkeyContext"))
    for name in (
        "ownGroundHotkey", "dispatchHotkeyGroup", "localHotkeyAction", "executeHotkey",
        "serviceHotkeyBatch", "handleHotkey",
    ):
        functions.append(function(hotkeys, name, optional=name == "localHotkeyAction"))
    actions = re.search(r"static const int actions\[\]\s*=\s*\{[^}]*\};", main)
    if not actions:
        raise ValueError("Production action table missing")
    return PREAMBLE + "\n" + actions.group(0) + "\n" + "\n".join(functions) + TESTS


def build_and_run(source: str, source_root: Path, out: Path, name: str, selections: tuple[str, ...]):
    cpp = out / f"{name}.cpp"
    exe = out / name
    cpp.write_text(source, encoding="utf-8")
    command = ["g++", "-std=c++17", "-O2", "-UNDEBUG", "-I", str(source_root / "source/assist/source"), str(cpp), "-o", str(exe)]
    built = subprocess.run(command, capture_output=True, text=True)
    (out / f"{name}-compile.log").write_text(built.stdout + built.stderr, encoding="utf-8")
    if built.returncode:
        raise RuntimeError(f"{name} failed to compile:\n{built.stdout}{built.stderr}")
    results = {}
    for selection in selections:
        ran = subprocess.run([str(exe), selection], capture_output=True, text=True)
        (out / f"{name}-{selection}.log").write_text(ran.stdout + ran.stderr, encoding="utf-8")
        try:
            result = json.loads(ran.stdout.strip().splitlines()[-1])
        except (ValueError, IndexError) as error:
            raise RuntimeError(f"{name}/{selection} returned no fixture result: {ran.stdout}{ran.stderr}") from error
        result["returncode"] = ran.returncode
        results[selection] = result
    return results


def run(args, out: Path) -> dict:
    root = args.source_root.resolve()
    main_path = "source/assist/source/main.cpp"
    hotkey_path = "source/assist/source/hotkey_host.h"
    bound_paths = (
        main_path, hotkey_path, "source/assist/source/hotkey_routes.h",
        "source/assist/source/control_link.h", "source/assist/source/protocol.h",
        "source/assist/source/read_only_lifecycle.h",
        "source/shared/source/key_gesture.h", "source/assist/source/additional_settings.h",
        "source/assist/source/type_hotkey.h", "source/assist/source/automatic_features.h",
        "source/assist/source/activation_diagnostic.h", "source/assist/tests/hotkey_command_context.py",
    ) + tuple(BASELINE_FIXTURES)
    source_bytes = {path: (root / path).read_bytes() for path in bound_paths}
    sources = {path: hashlib.sha256(content).hexdigest() for path, content in source_bytes.items()}
    for path, expected in BASELINE_FIXTURES.items():
        if sources[path] != expected:
            raise RuntimeError(f"Frozen pre.3 negative-control fixture changed: {path}")
    main = source_bytes[main_path].decode("utf-8-sig")
    hotkeys = source_bytes[hotkey_path].decode("utf-8-sig")
    current = build_and_run(program(main, hotkeys), root, out, "production", ("all",))["all"]
    if not current["passed"] or current["returncode"] != 0:
        raise RuntimeError(f"Production context regression failed: {current}")
    negative = {"requested": False}
    if args.negative_control_ref:
        if args.negative_control_ref != BASELINE_COMMIT:
            raise ValueError("Negative-control provenance must select the frozen pre.3 commit")
        ref = BASELINE_COMMIT
        before = {}
        for path in (main_path, hotkey_path):
            fixture = "source/assist/tests/fixtures/hotkey_pre1473/" + Path(path).name
            # Preserve the original extraction's universal-newline behavior;
            # the checked fixture and its reported source hash use raw bytes.
            before[path] = source_bytes[fixture].decode("utf-8-sig").replace("\r\n", "\n").replace("\r", "\n")
        results = build_and_run(program(before[main_path], before[hotkey_path]), root, out, "negative-control", ("registration", "message", "service"))
        expected = {
            "registration": "display failure must retain command hotkey mode",
            "message": "WM_HOTKEY must dispatch AutoBoard after display failure",
            "service": "pending hotkey must dispatch AutoBoard after display failure",
        }
        for case, failure in expected.items():
            result = results[case]
            if result["passed"] or result["returncode"] != 2 or result.get("failure") != failure:
                raise RuntimeError(f"Negative control did not reproduce the intended gate: {case}: {result}")
        negative = {
            "requested": True, "baseline_commit": ref, "reproduced": True,
            "cases": results,
            "scope": "baseline main.cpp/hotkey_host.h functions; current Registry shared with production case",
            "fixture_source_sha256": {path: sources[path] for path in BASELINE_FIXTURES},
            "extracted_source_sha256": {path: hashlib.sha256(text.encode()).hexdigest() for path, text in before.items()},
        }
    changed = [path for path, content in source_bytes.items() if (root / path).read_bytes() != content]
    if changed:
        raise RuntimeError(f"Production sources changed during this fixture: {changed}")
    result = {"passed": True, "checks": current["checks"], "scenarios": current["scenarios"], "negative_control": negative, "sources": sources, "scope": SCOPE}
    (out / "result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--out", type=Path)
    parser.add_argument("--negative-control-ref", choices=(BASELINE_COMMIT,), default=BASELINE_COMMIT)
    arguments = parser.parse_args()
    if arguments.out:
        destination = arguments.out.resolve()
        destination.mkdir(parents=True, exist_ok=True)
        result = run(arguments, destination)
    else:
        with tempfile.TemporaryDirectory(prefix="ra2-hotkey-context-") as directory:
            result = run(arguments, Path(directory))
    print(json.dumps(result, ensure_ascii=False))
