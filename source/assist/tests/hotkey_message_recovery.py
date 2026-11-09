"""Exercise the production shortcut registry against physical/OS event orderings.

The Win32 boundary is synthetic: this does not run a Windows message pump or
claim that a particular platform swallowed a user's WM_HOTKEY notification.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


BASELINE_COMMIT = "93f02787d42b61ff5c30e450317f99ba00673149"
BASELINE_SHA256 = "5f44086a95f720b361d34a11b31093ddfbaf9692b379e181fe7dc0d9fc5995ce"

PROGRAM = r'''
#include "hotkey_routes.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace assist::hotkeyroute;
static unsigned checks=0,cases=0;
static const char* scenario="";
static void check(bool okay,const char* text){++checks;if(!okay){std::cerr<<"FAIL "<<scenario<<": "<<text<<"\n";std::exit(1);}}
static void begin(const char* name){scenario=name;++cases;}
struct Fixture {
 Registry registry;std::array<uint8_t,256> keys{};std::vector<int> fired;uint32_t match=3;
 explicit Fixture(uint16_t key,uint32_t tick=100){check(registry.add(1011,key,true,[](int,uint16_t){return true;}),"binding accepted");registry.synchronize(keys,match,tick);}
 void add(int id,uint16_t key){check(registry.add(id,key,true,[](int,uint16_t){return true;}),"second binding accepted");}
 void emit(const Group& group){fired.insert(fired.end(),group.actions.begin(),group.actions.end());}
 void sample(uint32_t now,bool press=true){registry.sample(keys,now,match,[&](const Group&group){emit(group);},press);}
 bool message(uint32_t tick,uint32_t now,int id=1011){Group out;bool accepted=registry.message(id,tick,now,match,out);if(accepted)emit(out);return accepted;}
 bool expire(uint32_t now){Group out;bool result=registry.expired(now,match,out);if(result)emit(out);return result;}
 void count(size_t expected){check(fired.size()==expected,"exact number of actions");}
};
constexpr uint16_t Ctrl=0x200,Release=0x2000,Double=0x8000;
int main(){
 begin("ordinary key survives absent OS notification");{
  Fixture f('F'|Ctrl);f.keys[17]=f.keys['F']=1;f.sample(110);f.sample(115);f.count(1);
  f.keys['F']=0;f.sample(120);f.count(1);f.keys['F']=1;f.sample(130);f.count(2);
 }
 begin("OS first and sampler second");{
  Fixture f('A');f.keys['A']=1;check(f.message(110,111),"OS edge accepted");f.sample(112);f.count(1);
  f.keys['A']=0;f.sample(120);check(!f.message(110,125),"duplicate after release rejected");f.count(1);
 }
 begin("sampler first and delayed OS after release");{
  Fixture f('A');f.keys['A']=1;f.sample(110);check(!f.message(108,111),"same edge not repeated");
  f.keys['A']=0;f.sample(120);check(!f.message(108,125),"released edge not recreated");
  f.keys['A']=1;f.sample(130);check(!f.message(108,131),"older edge cannot replace newer one");f.count(2);
 }
 begin("ordinary double press with duplicate notifications");{
  Fixture f('A'|Double);f.keys['A']=1;f.sample(110);f.count(0);f.message(108,112);
  f.keys['A']=0;f.sample(120);f.message(108,125);f.count(0);
  f.keys['A']=1;f.sample(210);f.count(1);f.message(208,212);f.keys['A']=0;f.sample(220);
  check(!f.expire(600),"no delayed single remains");f.count(1);
 }
 begin("short OS press entirely between samples");{
  Fixture f('A');check(f.message(110,115),"short press accepted");f.sample(115);f.sample(200);f.count(1);
  check(f.message(210,215),"next short press accepted");f.sample(215);f.count(2);
 }
 begin("short OS press followed by observed release");{
  Fixture f('F'|Ctrl|Release);f.keys[17]=1;check(!f.message(110,115),"release binding does not fire on down");
  f.sample(115);f.count(1);f.message(110,120);f.count(1);
 }
 begin("double release uses both complete sampled taps");{
  Fixture f('F'|Ctrl|Release|Double);f.keys[17]=f.keys['F']=1;f.sample(110);f.keys['F']=0;f.sample(120);f.count(0);
  f.keys['F']=1;f.sample(210);f.message(208,211);f.count(0);f.keys['F']=0;f.sample(220);f.count(1);
 }
 begin("single versus double expiry uses input time");{
  Fixture f('A');f.add(1012,'A'|Double);f.keys['A']=1;f.sample(110);f.keys['A']=0;f.sample(120);f.count(0);
  check(!f.expire(460),"exact double window remains open");check(f.expire(461),"single expires after window");check(f.fired==std::vector<int>{1011},"single action selected");
  f.keys['A']=1;f.sample(600);f.keys['A']=0;f.sample(610);f.keys['A']=1;f.sample(650);f.keys['A']=0;f.sample(660);
  check(f.fired==std::vector<int>({1011,1012}),"second pair chooses double");check(!f.expire(1100),"double leaves no single");
 }
 begin("press and release actions keep separate phases");{
  Fixture f('A');f.add(1012,'A'|Release);f.keys['A']=1;f.sample(110);f.message(108,111);check(f.fired==std::vector<int>{1011},"press once");
  f.keys['A']=0;f.sample(120);check(f.fired==std::vector<int>({1011,1012}),"release once");f.message(108,130);f.count(2);
 }
 begin("new OS edge can prove a missed intervening release");{
  Fixture f('A');check(f.message(110,115),"first OS edge");check(f.message(210,215),"second distinct OS edge");f.sample(220);f.count(2);
 }
 begin("older queued message never becomes a new tap");{
  Fixture f('A');check(f.message(210,220),"new edge accepted");check(!f.message(110,221),"older edge discarded");f.count(1);
 }
 begin("message lifetime matches bounded command lifetime");{
  Fixture f('A');check(!f.message(110,2611),"2501ms old rejected");check(f.message(111,2611),"2500ms edge retained");f.count(1);
 }
 begin("future timestamp cannot cross freshness check");{
  Fixture f('A');check(!f.message(111,110),"future event rejected");f.count(0);
 }
 begin("registration while held cannot replay that hold");{
  Fixture f('A');f.keys['A']=1;f.registry.synchronize(f.keys,f.match,100);f.message(90,110);f.message(110,115);f.sample(120);f.count(0);
  f.keys['A']=0;f.sample(130);check(!f.message(110,135),"held-era late message rejected after release");
  f.keys['A']=1;f.sample(140);f.count(1);
 }
 begin("context cancellation discards old tap and held key");{
  Fixture f('A'|Double);f.keys['A']=1;f.sample(110);f.registry.cancel(120);check(!f.message(110,130),"cancelled notification rejected");
  f.sample(150);f.keys['A']=0;f.sample(160);f.keys['A']=1;f.sample(170);f.count(0);f.keys['A']=0;f.sample(180);f.keys['A']=1;f.sample(210);f.count(1);
 }
 begin("timed cancellation requires fresh physical context before OS input");{
  Fixture f('A');f.registry.cancel(100);check(!f.message(110,120),"inactive-gap message cannot revive before sampling");f.sample(151);
  check(f.message(160,161),"fresh edge after context resynchronization");f.count(1);
 }
 begin("untimed compatibility cancellation requires a fresh sample");{
  Fixture f('A');f.registry.cancel();check(!f.message(150,151),"no timestamp boundary means no replay");f.sample(160);
  check(f.message(170,171),"fresh synchronized edge accepted");f.count(1);
 }
 begin("match change observed by sampler suppresses old hold");{
  Fixture f('A');f.keys['A']=1;f.sample(110);f.match=4;f.sample(120);check(!f.message(110,125),"previous-match edge discarded");f.count(1);
  f.keys['A']=0;f.sample(130);f.keys['A']=1;f.sample(140);f.count(2);
 }
 begin("match change observed first by message establishes fence");{
  Fixture f('A');f.match=4;check(!f.message(150,151),"queued edge cannot authenticate new match");
  check(!f.message(153,154),"new match needs physical synchronization");f.sample(155);
  check(f.message(160,161),"later edge belongs to established match");f.count(1);
 }
 begin("message deduplication crosses tick wrap");{
  Fixture f('A',0xffffffe0u);f.keys['A']=1;f.sample(0xfffffff0u);check(!f.message(0xffffffefu,5),"old pre-wrap message rejected");
  f.keys['A']=0;f.sample(6);check(f.message(7,8),"new post-wrap message accepted");f.count(2);
 }
 begin("context fence crosses tick wrap");{
  Fixture f('A',0xffffffe0u);f.registry.cancel(0xfffffff8u);f.sample(2);check(!f.message(0xfffffff0u,10),"old context message rejected across wrap");
  check(f.message(4,10),"new context message accepted across wrap");f.count(1);
 }
 begin("poll and OS timestamp in same tick execute once");{
  Fixture f('A');f.keys['A']=1;f.sample(110);check(!f.message(110,110),"same-tick notification is duplicate");f.count(1);
 }
 begin("two sampled physical edges in same tick remain distinct");{
  Fixture f('A');f.keys['A']=1;f.sample(110);f.keys['A']=0;f.sample(110);f.keys['A']=1;f.sample(110);f.count(2);
  f.message(110,111);f.count(2);
 }
 begin("modifier loss cancels release qualification");{
  Fixture f('F'|Ctrl|Release);f.keys[17]=f.keys['F']=1;f.sample(110);f.keys[17]=0;f.sample(120);f.keys[17]=1;f.sample(130);f.keys['F']=0;f.sample(140);f.count(0);
  f.keys['F']=1;f.sample(150);f.keys['F']=0;f.sample(160);f.count(1);
 }
 begin("adding prefix to an already held key is not a press");{
  Fixture f('F'|Ctrl);f.keys['F']=1;f.sample(110);f.keys[17]=1;f.sample(120);check(!f.message(120,121),"wrong-order held chord suppressed");f.count(0);
  f.keys['F']=0;f.sample(130);f.keys['F']=1;f.sample(140);f.count(1);
 }
 begin("removing extra prefix cannot resurrect held key");{
  Fixture f('F'|Ctrl);f.keys[16]=f.keys[17]=f.keys['F']=1;f.sample(110);f.keys[16]=0;f.sample(120);f.count(0);
  f.keys['F']=0;f.sample(130);f.keys['F']=1;f.sample(140);f.count(1);
 }
 begin("bare modifiers and mouse retain physical edge route");{
  for(uint16_t key:{uint16_t(17),uint16_t(5)}){Fixture f(key);f.keys[key]=1;f.sample(110);f.sample(120);f.count(1);f.keys[key]=0;f.sample(130);f.keys[key]=1;f.sample(140);f.count(2);}
 }
 begin("explicit poll suppression leaves OS route available");{
  Fixture f('A');f.keys['A']=1;f.sample(110,false);f.count(0);check(f.message(112,113),"OS route accepted");f.sample(114,false);f.count(1);
 }
 begin("rebinding same slot rejects previous registration messages");{
  Fixture f('A');f.registry.clear();check(f.registry.add(1011,'B',true,[](int,uint16_t){return true;}),"replacement added");f.registry.synchronize(f.keys,f.match,200);
  check(!f.message(150,210),"previous registration timestamp rejected");f.keys['B']=1;f.sample(220);f.count(1);
 }
 begin("unknown slot and missing match cannot dispatch");{
  Fixture f('A');check(!f.message(110,111,9999),"unknown slot");f.match=0;check(!f.message(120,121),"match zero");f.count(0);
 }
 begin("genuine second OS tap after polled first tap");{
  Fixture f('A'|Double);f.keys['A']=1;f.sample(110);f.keys['A']=0;f.sample(120);check(!f.message(108,130),"delayed first OS is not second tap");f.count(0);
  check(f.message(210,215),"genuine second OS edge accepted");f.sample(220);f.count(1);
 }
 begin("same-tick context boundary rejects ambiguous queued edge");{
  Fixture f('A');f.registry.cancel(120);f.sample(120);check(!f.message(120,120),"context-boundary equality fails closed");check(f.message(121,121),"later edge accepted");f.count(1);
 }
 begin("OS queue stamp slightly after physical sample is acknowledgement");{
  Fixture f('A',90);f.keys['A']=1;f.sample(100);check(!f.message(101,105),"slightly later queue stamp is same press");f.count(1);
  f.sample(110);check(!f.message(101,115),"repeated queued notification stays consumed");f.count(1);
 }
 begin("paired first OS notification preserves subsequent short second press");{
  Fixture f('A',90);f.keys['A']=1;f.sample(100);f.message(101,105);f.count(1);
  check(f.message(150,155),"distinct second OS edge accepted without intervening sample");f.keys['A']=0;f.sample(160);f.count(2);
 }
 begin("unpaired poll followed by observed release admits newer short OS press");{
  Fixture f('A',90);f.keys['A']=1;f.sample(100);f.keys['A']=0;f.sample(110);
  check(f.message(120,125),"OS edge after confirmed release is new press");f.sample(126);f.count(2);
 }
 begin("late first notification after sampled release remains acknowledgement");{
  Fixture f('A',90);f.keys['A']=1;f.sample(100);f.keys['A']=0;f.sample(110);
  check(!f.message(101,115),"late notification before release boundary is same press");f.count(1);
 }
 begin("stale first notification cannot steal newer poll acknowledgement");{
  Fixture f('A',90);f.keys['A']=1;f.sample(100);f.keys['A']=0;f.sample(110);f.keys['A']=1;f.sample(200);
  check(!f.message(101,201),"previous press notification ignored");check(!f.message(201,205),"current press still pairs its own notification");f.count(2);
 }
 begin("inactive-gap notification remains rejected after resynchronization");{
  Fixture f('A');f.registry.cancel(120);check(!f.message(130,140),"no replay before resync");f.sample(150);
  check(!f.message(130,155),"no replay after resync");check(f.message(160,161),"new live input accepted");f.count(1);
 }
 begin("Windows prefix cannot trigger a plain command");{
  for(unsigned win:{91u,92u}){Fixture f('A');f.keys[win]=f.keys['A']=1;f.sample(110);f.count(0);
   f.keys[win]=0;f.sample(120);f.count(0);f.keys['A']=0;f.sample(130);f.keys['A']=1;f.sample(140);f.count(1);}
 }
 begin("Windows prefix cannot trigger an otherwise matching chord");{
  Fixture f('F'|Ctrl);f.keys[17]=f.keys[91]=f.keys['F']=1;f.sample(110);f.count(0);
  f.keys[91]=0;f.sample(120);f.count(0);f.keys['F']=0;f.sample(130);f.keys['F']=1;f.sample(140);f.count(1);
 }
 begin("physical sample freshness has exact bounded lifetime");{
  Fixture f('A');check(f.registry.sampleFresh(250),"150ms sampling interval is fresh");check(!f.registry.sampleFresh(251),"151ms sampling gap is stale");
  check(!f.registry.sampleFresh(99),"future sample timestamp is not fresh");f.message(110,111);check(!f.registry.sampleFresh(251),"OS notification does not renew physical sample time");
  f.sample(300);check(f.registry.sampleFresh(450),"sample renews physical timestamp");check(!f.registry.sampleFresh(451),"new sample still expires");
 }
 begin("cancel clear and untimed sync cannot manufacture fresh input");{
  Fixture f('A');f.registry.cancel(110);check(!f.registry.sampleFresh(110),"cancel clears sample freshness");
  f.registry.synchronize(f.keys,f.match);check(!f.registry.sampleFresh(120),"untimed synchronization remains unknown");
  f.registry.synchronize(f.keys,f.match,130);check(f.registry.sampleFresh(130),"timed synchronization establishes sample");
  f.registry.clear();check(!f.registry.sampleFresh(130),"clear removes sample freshness");
 }
 begin("physical sample freshness handles uptime wrap");{
  Fixture f('A',0xfffffff0u);check(f.registry.sampleFresh(0x86),"150ms interval across wrap is fresh");check(!f.registry.sampleFresh(0x87),"151ms interval across wrap is stale");
  f.sample(0x90);check(f.registry.sampleFresh(0x90),"post-wrap sample establishes new timestamp");
 }
 begin("message-observed epoch change invalidates physical sample freshness");{
  Fixture f('A');check(f.registry.sampleFresh(110),"initial epoch sampled");f.match=4;f.message(110,111);
  check(!f.registry.sampleFresh(111),"epoch transition clears old sampling timestamp");f.sample(112);check(f.registry.sampleFresh(112),"new physical sample restores freshness");
 }
 std::cout<<"{\"case_count\":"<<cases<<",\"check_count\":"<<checks<<",\"passed\":true}\n";
}
'''

NEGATIVE = r'''
#include "hotkey_routes.h"
#include <cstdint>
#include <iostream>
int main(){
 assist::hotkeyroute::Registry registry;uint8_t keys[256]{};int fired=0;
 registry.add(1011,'F'|0x200,false,[](int,uint16_t){return true;});registry.synchronize(keys,3);
 keys[17]=keys['F']=1;registry.sample(keys,110,3,[&](const assist::hotkeyroute::Group&out){fired+=int(out.actions.size());});
 keys['F']=0;registry.sample(keys,120,3,[&](const assist::hotkeyroute::Group&out){fired+=int(out.actions.size());});
 std::cout<<(fired==1?"PASS":"FAIL")<<" ordinary_without_wm expected=1 actual="<<fired<<"\n";return fired==1?0:1;
}
'''


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compile_and_run(directory: Path, name: str, program: str, include: Path) -> dict:
    cpp = directory / (name + ".cpp")
    binary = directory / name
    cpp.write_text(program, encoding="utf-8")
    command = ["g++", "-std=c++17", "-O2", "-I", str(include), str(cpp), "-o", str(binary)]
    compiled = subprocess.run(command, text=True, capture_output=True)
    if compiled.returncode:
        raise RuntimeError("C++ compilation failed: " + compiled.stderr)
    result = subprocess.run([str(binary)], text=True, capture_output=True)
    return {"exit_code": result.returncode, "stdout": result.stdout.strip(), "stderr": result.stderr.strip()}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    root = args.source_root.resolve()
    registry = root / "source/assist/source/hotkey_routes.h"
    gestures = root / "source/shared/source/key_gesture.h"
    baseline = root / "source/assist/tests/fixtures/hotkey_pre1473/hotkey_routes.h"
    script = root / "source/assist/tests/hotkey_message_recovery.py"
    if digest(baseline) != BASELINE_SHA256:
        raise RuntimeError("Frozen pre.3 negative-control source changed")
    with tempfile.TemporaryDirectory(prefix="ra2-hotkey-messages-") as scratch:
        work = Path(scratch)
        result = compile_and_run(work, "current", PROGRAM, registry.parent)
        if result["exit_code"]:
            raise RuntimeError(result["stderr"] or result["stdout"])
        counts = json.loads(result["stdout"])
        current_probe = compile_and_run(work, "current_probe", NEGATIVE, registry.parent)
        old_include = work / "baseline/source/assist/source"
        old_shared = work / "baseline/source/shared/source"
        old_include.mkdir(parents=True)
        old_shared.mkdir(parents=True)
        shutil.copyfile(baseline, old_include / "hotkey_routes.h")
        shutil.copyfile(gestures, old_shared / "key_gesture.h")
        old_probe = compile_and_run(work, "baseline_probe", NEGATIVE, old_include)
        if current_probe["exit_code"] != 0 or old_probe["exit_code"] != 1 or "actual=0" not in old_probe["stdout"]:
            raise RuntimeError("Missing-message regression did not distinguish frozen production baseline")
    report = {
        "test": "hotkey_message_recovery",
        "passed": True,
        **counts,
        "production_run": result,
        "current_missing_message_probe": current_probe,
        "baseline_negative_control": {
            "commit": BASELINE_COMMIT,
            "sha256": BASELINE_SHA256,
            "expected_failure": True,
            **old_probe,
        },
        "source_hashes": {str(path.relative_to(root)): digest(path) for path in (registry, gestures, baseline, script)},
        "scope": "Production Registry with synthetic key states and message timestamps; not native Windows, a game process, GPU or multiplayer validation.",
    }
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"PASS hotkey_message_recovery: {counts['case_count']} cases, {counts['check_count']} checks; frozen pre.3 missing-message control fails as expected")


if __name__ == "__main__":
    main()
