#!/usr/bin/env python3
"""Enhanced native mouse pairing through the production Runtime message route.

The first owned press is established by actual resolveMouse and the complete
AssistFilterMessage, not by directly seeding its suppressed-button mask. The
Host policy publication statement and GameRuntime forwarding selector are
also taken from production. Native world/memory, platform input delivery,
established controller lease and unrelated inactive features are explicit
controlled seams. This is a Linux fixture, not Windows or game acceptance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from pause_key_pairing import PREAMBLE
from superweapon_result_feedback import function


def replace_once(text, before, after):
    if text.count(before) != 1:
        raise ValueError('Shared fixture seam changed: ' + before)
    return text.replace(before, after)


def preamble():
    result = PREAMBLE
    for before, after in (
        ('static bool nativeReady(){return true;}',
         'static bool nativeAvailable=true;static bool nativeReady(){return nativeAvailable;}'),
        ('static int outgoingRoom(){return 1;}',
         'static int room=1;static int outgoingRoom(){return room;}'),
        ('static bool pointerLocation(pointerinput::Intent&){return false;}',
         'static bool locationAvailable=true;static bool pointerLocation(pointerinput::Intent&){pointerTarget=0;pointerCell=0x500000;return locationAvailable;}'),
        ('struct Vec{uint32_t items=0;int count=0;};static Vec vec(uint32_t){return {};}',
         'struct Vec{uint32_t items=0;int count=0;};static int selectedCount=1;static Vec vec(uint32_t){return {0x300000,selectedCount};}'),
        ('static uint32_t item(Vec,int){return 0;}',
         'static uint32_t item(Vec,int index){return index==0?0x400000:0;}'),
        ('static struct{bool contains(uint32_t){return false;}}worldObjects;',
         'static struct{bool contains(uint32_t p){return p==0x400000;}}worldObjects;'),
        ('static bool own(uint32_t,uint32_t){return false;}',
         'static bool own(uint32_t p,uint32_t house){return p==0x400000&&house==lastHouse;}'),
        ('static int kind(uint32_t){return 0;}',
         'static int kind(uint32_t){return 1;}'),
        ('static bool rawArmed(uint32_t){return false;}',
         'static bool rawArmed(uint32_t p){return p==0x400000;}'),
    ):
        result = replace_once(result, before, after)
    return result + r'''
static uint32_t commandMatch(){return match;}
static uint32_t InterlockedIncrement(uint32_t*p){return ++*p;}
static struct{uint32_t overrideNative=1;uint32_t mouseButtons[pointerinput::MouseCount]{};}extra;
'''


CASES = r'''
static unsigned checks=0,failures=0;
static void check(bool value,const char*name){++checks;if(!value){++failures;std::cerr<<"FAIL "<<name<<"\n";}}
static unsigned down(unsigned physical){return physical==6?WM_LBUTTONDOWN:WM_RBUTTONDOWN;}
static unsigned up(unsigned physical){return physical==6?WM_LBUTTONUP:WM_RBUTTONUP;}
static unsigned vk(unsigned physical){return physical==6?VK_LBUTTON:VK_RBUTTON;}
static uint32_t bit(unsigned physical){return physical==6?1u:2u;}
static void reset(unsigned physical=6){
 storage={};sh=&storage;sh->moFeatures=0;memory.clear();std::memset(physicalKeys,0,sizeof physicalKeys);
 runtimeKeys.clear();pauseViewState={};pauseViewHotkeys.clear();pauseViewKeyEpoch=0;
 std::memset(pauseViewKeySuppressed,0,sizeof pauseViewKeySuppressed);
 pauseViewInstalled=pauseViewNativeFailed=false;clockNow=100;match=1;fault=false;
 blockedButtons=0;std::memset(blockedOrigins,0,sizeof blockedOrigins);
 nativeAvailable=locationAvailable=true;room=selectedCount=1;
 pointerCell=pointerTarget=pointerTargetID=0;pointerGroundZ=0;
 memory[0x887324]=0x200000;memory[0x8A00A4]=800;memory[0x8A00A8]=600;
 extra={};extra.overrideNative=1;extra.mouseButtons[pointerinput::MouseAttack]=physical;
 publishHostPolicy(true);
}
static unsigned deliver(unsigned type){
 ++clockNow;sh->heartbeat=clockNow;
 if(type==WM_LBUTTONDOWN||type==WM_LBUTTONDBLCLK)physicalKeys[VK_LBUTTON]=1;
 if(type==WM_RBUTTONDOWN||type==WM_RBUTTONDBLCLK)physicalKeys[VK_RBUTTON]=1;
 if(type==WM_LBUTTONUP)physicalKeys[VK_LBUTTON]=0;
 if(type==WM_RBUTTONUP)physicalKeys[VK_RBUTTON]=0;
 MSG msg{1,type,0,100|(200<<16)};deliverRuntimeMessage(&msg);return msg.message;
}
static void loseRelease(unsigned physical){
 check(deliver(down(physical))==WM_NULL&&(blockedButtons&bit(physical))&&
       sh->suppressedClick.match==match&&sh->suppressedClick.physical==physical,
       "configured AttackMove owns the initial physical down through production routing");
 sh->foreground=false;publishHostPolicy(false);deliver(WM_MOUSEMOVE);
 // Alt-tab/focus loss while held; Windows releases capture. The physical UP
 // goes to the other window, so the game hook does not receive that message.
 physicalKeys[vk(physical)]=0;
 check(runtimeKeys.held[vk(physical)]==0&&(blockedButtons&bit(physical)),
       "real outer focus-loss path clears Tracker but retains old generic ownership");
 sh->foreground=true;publishHostPolicy(true);
}
static bool ordinaryPair(unsigned physical){
 const bool d=deliver(down(physical))==down(physical);
 const bool u=deliver(up(physical))==up(physical);
 return d&&u;
}
int main(){
 reset();check(deliver(WM_LBUTTONDOWN)==WM_NULL,"normal enhanced left down is suppressed");
 check(deliver(WM_LBUTTONUP)==WM_NULL&&!blockedButtons,"normal enhanced left up balances its own down");
 check(deliver(WM_LBUTTONUP)==WM_LBUTTONUP,"unmatched up remains native");
 reset(4);check(deliver(WM_RBUTTONDOWN)==WM_NULL,"normal enhanced right down is suppressed");
 check(deliver(WM_RBUTTONUP)==WM_NULL&&!blockedButtons,"normal enhanced right up balances its own down");

 reset();check(deliver(WM_LBUTTONDOWN)==WM_NULL,"owned press before same-hold disable");
 extra.overrideNative=0;publishHostPolicy(true);
 check(deliver(WM_LBUTTONUP)==WM_NULL,"disable without a new down retains original release ownership");
 check(ordinaryPair(6),"subsequent ordinary left pair is untouched");

 reset();loseRelease(6);extra.overrideNative=0;publishHostPolicy(true);
 check(ordinaryPair(6),"outside release then disable preserves new native left pair");
 reset(4);loseRelease(4);extra.overrideNative=0;publishHostPolicy(true);
 check(ordinaryPair(4),"outside release then disable preserves new native right pair");
 reset();loseRelease(6);extra.mouseButtons[pointerinput::MouseAttack]=2;publishHostPolicy(true);
 check(ordinaryPair(6),"outside release then mouse rebinding preserves new native pair");
 reset();loseRelease(6);sh->pointer.request=1;
 check(ordinaryPair(6),"new native click rejected by busy mailbox retains its release");
 reset();loseRelease(6);selectedCount=0;
 check(ordinaryPair(6),"new native click with unsupported empty selection retains its release");
 reset();loseRelease(6);nativeAvailable=false;
 check(ordinaryPair(6),"new native click while native context unavailable retains its release");
 reset();loseRelease(6);publishHostPolicy(false);
 check(ordinaryPair(6),"new native click before Host input readiness retains its release");

 reset();loseRelease(6);
 check(deliver(WM_LBUTTONDOWN)==WM_NULL&&deliver(WM_LBUTTONUP)==WM_NULL&&!blockedButtons,
       "unchanged active feature reclaims the new physical pair");
 reset();check(deliver(WM_LBUTTONDOWN)==WM_NULL,"left ownership established before independent right click");
 extra.overrideNative=0;publishHostPolicy(true);
 check(ordinaryPair(4)&&(blockedButtons&1),"new right pair does not clear existing left ownership");
 check(deliver(WM_LBUTTONUP)==WM_NULL&&!blockedButtons,"left original release still balances after right pair");
 reset();deliver(WM_LBUTTONDOWN);deliver(WM_LBUTTONUP);
 check(deliver(WM_LBUTTONDBLCLK)==WM_LBUTTONDBLCLK&&deliver(WM_LBUTTONUP)==WM_LBUTTONUP,
       "non-cooperative native double-click keeps both physical edges");
 std::cout<<"{\"checks\":"<<checks<<",\"failures\":"<<failures<<",\"passed\":"<<(failures?"false":"true")<<"}\n";
 return failures?1:0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument('--message', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    files = {
        'message': args.message or args.source_root / 'source/assist/source/native_message_policy.h',
        'host': args.source_root / 'source/assist/source/pointer_host.h',
        'runtime': args.source_root / 'source/game_runtime/source/runtime.cpp',
        'pause': args.source_root / 'source/assist/source/pause_view_bridge.h',
        'registry': args.source_root / 'source/assist/source/hotkey_routes.h',
        'tracker': args.source_root / 'source/shared/source/key_gesture.h',
        'policy': args.source_root / 'source/assist/source/pause_view_policy.h',
        'pointer': args.source_root / 'source/assist/source/pointer_intent.h',
        'test': Path(__file__).resolve(),
        'fixture_helper': Path(__file__).with_name('pause_key_pairing.py'),
        'extractor': Path(__file__).with_name('superweapon_result_feedback.py'),
    }
    captured = {key: path.read_bytes() for key, path in files.items()}
    text = {key: data.decode('utf-8') for key, data in captured.items()}
    pause = text['pause'][:text['pause'].index('// Each non-pumping native draw/input')]
    bodies = [pause, function(text['pause'], 'static void pauseViewTick(){'),
              function(text['message'], 'static void recordRuntimeGesture('),
              function(text['message'], 'extern "C" int AssistFilterMessage('),
              function(text['message'], 'extern "C" void AssistRuntimeFilter(')]
    allowed = [line.strip() for line in text['runtime'].splitlines()
               if 'if((control->enabled&runtime::Tactical)' in line and 'AssistRuntimeFilter((void*)lp);' in line]
    published = [line.strip() for line in text['host'].splitlines()
                 if 'InterlockedIncrement(&shared->pointerPolicySeq);shared->pointerPolicy={};' in line]
    if len(allowed) != 1 or len(published) != 1:
        raise ValueError('Production outer route or Host publication changed; review fixture')
    bodies.append('static void deliverRuntimeMessage(MSG*msg){auto lp=reinterpret_cast<LPARAM>(msg);' + allowed[0] + '}')
    bodies.append('static void publishHostPolicy(bool inputReady){auto*shared=sh;pointerinput::Bindings bindings{};' + published[0] + '}')
    args.out.mkdir(parents=True, exist_ok=True)
    inputs = args.out / 'inputs'
    inputs.mkdir(exist_ok=True)
    for key, data in captured.items():
        (inputs / (key + '.source')).write_bytes(data)
    frozen = args.out / 'frozen/source'
    for key, relative in (('registry', 'assist/source/hotkey_routes.h'),
                          ('tracker', 'shared/source/key_gesture.h'),
                          ('policy', 'assist/source/pause_view_policy.h'),
                          ('pointer', 'assist/source/pointer_intent.h')):
        target = frozen / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(captured[key])
    cpp, binary = args.out / 'fixture.cpp', args.out / 'fixture'
    cpp.write_text(preamble() + '\n' + '\n'.join(bodies) + '\n' + CASES)
    command = ['g++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-I' + str(frozen / 'assist/source'), str(cpp), '-o', str(binary)]
    compiled = subprocess.run(command, text=True, capture_output=True)
    (args.out / 'compile.log').write_text(compiled.stdout + compiled.stderr)
    run = subprocess.run([str(binary)], text=True, capture_output=True) if not compiled.returncode else None
    if run:
        (args.out / 'run.log').write_text(run.stdout + run.stderr)
    receipt = {
        'test': 'production generic mouse pairing via Host policy and Runtime outer filters',
        'compile_command': command, 'compile_returncode': compiled.returncode,
        'run_command': [str(binary)], 'run_returncode': run.returncode if run else None,
        'source_sha256': {key: {'path': str(path), 'sha256': hashlib.sha256(captured[key]).hexdigest()} for key, path in files.items()},
        'function_sha256': [hashlib.sha256(body.encode()).hexdigest() for body in bodies],
        'scope': 'Linux controlled production input path; Win32 delivery, native memory/patch and unrelated inactive features are seams',
        'windows_executed': False, 'game_executed': False,
        'result': json.loads(run.stdout) if run and run.stdout.startswith('{') else None,
    }
    (args.out / 'receipt.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(receipt, ensure_ascii=False))
    raise SystemExit(compiled.returncode or (run.returncode if run else 1))


if __name__ == '__main__':
    main()
