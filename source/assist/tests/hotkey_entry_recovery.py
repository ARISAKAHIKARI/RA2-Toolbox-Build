#!/usr/bin/env python3
"""Run extracted production input entry paths with controlled OS/UI endpoints.

The actual Host, middle ownership and formation fallback implementations
are executed. UI owner messages and final command/watch sinks are observations,
not replacements for admission logic. This is not Windows or game execution.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]

EXTRA = r'''
constexpr unsigned VK_SHIFT=16,VK_CONTROL=17,VK_MENU=18;
using LONG=int32_t;
constexpr int BM_GETCHECK=1,BM_SETCHECK=2,BST_CHECKED=1,BST_UNCHECKED=0;
constexpr unsigned WM_COMMAND=0x111,BN_CLICKED=0;
static uint32_t settingsRevision=1,pid=72;
static unsigned hook=1,wakes=0,automaticCalls=0,watchCalls=0;
static bool ready=true,controlEnabled=true;
static bool controlChecked[10000]{};
static uintptr_t MAKEWPARAM(unsigned low,unsigned high){return low|(uintptr_t(high)<<16);}
static void MemoryBarrier(){}
static LONG InterlockedIncrement(volatile LONG*value){return ++*value;}
static void wakeGame(){++wakes;}
static uint32_t sampleClock(const Shared*){return now;}
static HWND GetDlgItem(HWND,int id){return reinterpret_cast<HWND>(uintptr_t(id));}
static bool IsWindowEnabled(HWND){return controlEnabled;}
static intptr_t SendMessageW(HWND control,unsigned message,WPARAM value,LPARAM){
 if(message==BM_GETCHECK)return controlChecked[uintptr_t(control)]?BST_CHECKED:BST_UNCHECKED;
 if(message==BM_SETCHECK)controlChecked[uintptr_t(control)]=value==BST_CHECKED;
 return 0;
}
static intptr_t proc(HWND,unsigned message,WPARAM action,LPARAM){
 if(message==WM_COMMAND&&autofeatures::byControl(int(LOWORD(action))))++automaticCalls;
 return 0;
}
namespace gameinput {static bool keySnapshot(unsigned,uint8_t(&keys)[256]){std::copy(rawKeys,rawKeys+256,keys);return true;}}
'''

TESTS = r'''
static unsigned checks=0,failures=0,cases=0;
static const char*caseName="";
static void check(bool okay,const char*reason){++checks;if(!okay){++failures;std::cerr<<caseName<<": "<<reason<<"\n";}}
static void resetCase(const char*name){
 ++cases;caseName=name;now+=5000;lastData=messageAt=now;++pid;++settingsRevision;
 storage=std::make_unique<Shared>();shared=storage.get();incoming={};incoming.status=incoming.bridgeStatus=incoming.commandStatus=1;incoming.match=8;
 snap=incoming;snap.renderStatus=snap.readStatus=1;
 enabled=focused=ready=controlEnabled=true;hasSelection=keysRegistered=false;registeredHotkeyMode=0;
 formationOwns=middleOwns=false;osInstallOkay=true;hookSmartUnload=hookAutoCS=buildAnywhereMode=false;
 extra={};extra.allHotkeys=false;extra.stickyKey=extra.variantKey=0;extra.formationEnabled=false;extra.formationHold=0;
 for(auto&key:extra.missionModeKeys)key=0;for(auto&key:extra.watchKeys)key=0;for(auto&key:extra.formationKeys)key=0;
 std::memset(hotkeys,0,sizeof hotkeys);std::memset(rawKeys,0,sizeof rawKeys);std::memset(controlChecked,0,sizeof controlChecked);
 std::memset(socialKeys,0,sizeof socialKeys);ejectKey=unloadKey=csKey=idleAircraftKey=0;
 commandQueue.size=0;typeBindings.clear();swBindings.clear();notifications.clear();commands.clear();
 registeredKeyIDs.clear();osRegistered.clear();hotkeyRegistry.clear();hotkeyPending.clear();
 typeTap.clear();variantTap.clear();formationTap.clear();hotkeyDiagnostics={};
 destructSubmissions=placementSubmissions=wakes=automaticCalls=watchCalls=0;
 selectionCalls=0;selectionGlobal=false;std::memset(selectionMask,0,sizeof selectionMask);
}
static void advance(unsigned ms){now+=ms;lastData=now;}
static void message(int id,char key){advance(1);messageAt=now;rawKeys[17]=rawKeys[unsigned(key)]=1;handleHotkey(id,LPARAM((unsigned(key)<<16)|MOD_CONTROL));}
static void release(char key){advance(1);rawKeys[unsigned(key)]=0;serviceHotkeyBatch();rawKeys[17]=0;}
static void report(unsigned before){
 std::cout<<"{\"case\":\""<<caseName<<"\",\"passed\":"<<(before==failures?"true":"false")
 <<",\"commands\":"<<commands.size()<<",\"automatic\":"<<automaticCalls<<",\"watch\":"<<watchCalls
 <<",\"formation_requests\":"<<shared->formationKey<<",\"pending\":"<<hotkeyPending.count
 <<",\"blocked\":"<<hotkeyDiagnostics.blocked<<"}\n";
}
static void formationCases(){
 resetCase("formation_direct_positive_control");unsigned before=failures;
 extra.formationEnabled=true;extra.formationKeys[0]='F'|0x200;
 shared->formationPreview.active=1;shared->formationPreview.match=8;
 check(formationHotkey('F'|0x200),"shape dispatcher should recognize the configured chord");
 check(shared->formationKey==1,"direct shape handoff must increment its mailbox");report(before);

 resetCase("formation_current_match_wm");before=failures;
 extra.formationEnabled=true;extra.formationKeys[0]='F'|0x200;hotkeys[11]='F'|0x200;
 shared->formationPreview.active=1;shared->formationPreview.match=8;registerKeys();message(1011,'F');
 check(shared->formationKey==1,"valid current formation WM_HOTKEY must reach shape handoff");
 check(commands.empty(),"formation owns the chord, AutoBoard must not run");report(before);

 resetCase("formation_old_match_must_not_steal");before=failures;
 extra.formationEnabled=true;extra.formationKeys[0]='F'|0x200;hotkeys[11]='F'|0x200;
 shared->formationPreview.active=1;shared->formationPreview.match=7;registerKeys();message(1011,'F');
 check(shared->formationKey==0,"old-match preview must not steal a current input");
 check(commands.size()==1&&commands[0]==AutoBoard,"current command must survive old preview");report(before);

 resetCase("formation_expired_wm_rejected");before=failures;
 extra.formationEnabled=true;extra.formationKeys[0]='F'|0x200;hotkeys[11]='F'|0x200;
 shared->formationPreview.active=1;shared->formationPreview.match=8;registerKeys();advance(201);messageAt=now-200;
 handleHotkey(1011,LPARAM(('F'<<16)|MOD_CONTROL));check(shared->formationKey==0&&commands.empty(),"expired shape message must remain rejected");report(before);

 resetCase("formation_wrong_binding_rejected");before=failures;
 extra.formationEnabled=true;extra.formationKeys[0]='G'|0x200;hotkeys[11]='F'|0x200;
 shared->formationPreview.active=1;shared->formationPreview.match=8;registerKeys();message(1011,'G');
 check(shared->formationKey==0&&commands.empty(),"old registered binding must not enter a shape action");report(before);

 resetCase("formation_lease_blocks_immediate_wm");before=failures;
 extra.missionModeKeys[selectedmode::OwnGround]='F'|0x200;
 shared->formationPreview.match=8;shared->formationConsumedUntil=now+200;registerKeys();message(1724+selectedmode::OwnGround,'F');
 check(commands.empty(),"formation post-gesture lease must block an immediate selection action");report(before);
}
static void busyCases(){
 for(int busy=0;busy<4;++busy){
  const char*names[]={"automatic_busy_command","automatic_busy_selection","automatic_pending_selection","automatic_queued_command"};
  resetCase(names[busy]);unsigned before=failures;hotkeys[0]='R'|0x200;registerKeys();
  if(busy==0)shared->request=1;if(busy==1)shared->selectRequest=1;if(busy==2)hasSelection=true;if(busy==3)commandQueue.size=1;
  message(1000,'R');release('R');for(int tick=0;tick<27;++tick){advance(100);serviceHotkeyBatch();}
  check(automaticCalls==1&&controlChecked[100],"local toggle must not expire behind an unrelated mailbox");
  check(hotkeyPending.count==0,"local action must be consumed exactly once");
  shared->ack=shared->request;shared->selectAck=shared->selectRequest;hasSelection=false;commandQueue.size=0;
  advance(1);serviceHotkeyBatch();check(automaticCalls==1,"unblocking commands must not replay local toggle");report(before);
 }
 resetCase("automatic_free_positive_control");unsigned before=failures;hotkeys[0]='R'|0x200;registerKeys();message(1000,'R');
 check(automaticCalls==1&&controlChecked[100],"local toggle works when command mailboxes are idle");report(before);

 resetCase("watch_busy_command");before=failures;extra.watchEnabled=true;extra.watchKeys[0]='W'|0x200;registerKeys();shared->request=1;message(1730,'W');
 check(watchCalls==1,"read-only pin dispatch must not wait for command ACK");report(before);

 resetCase("sidebar_busy_command");before=failures;extra.sidebarEnabled=true;extra.sidebarKey='B'|0x200;registerKeys();shared->request=1;message(1734,'B');
 check(extra.sidebarMode==1,"local sidebar setting must not wait for command ACK");report(before);

 resetCase("fanout_local_once_command_waits");before=failures;extra.allHotkeys=true;hotkeys[0]=hotkeys[11]='R'|0x200;
 extra.watchEnabled=true;extra.watchKeys[0]='R'|0x200;registerKeys();shared->request=1;message(1000,'R');
 check(automaticCalls==1&&watchCalls==1&&commands.empty(),"fanout local actions must execute while game action remains queued");
 release('R');advance(10);serviceHotkeyBatch();check(automaticCalls==1&&watchCalls==1,"same fanout must not repeat completed local actions");
 shared->ack=shared->request;advance(10);serviceHotkeyBatch();check(commands.size()==1&&commands[0]==AutoBoard,"fanout game action must proceed once its mailbox frees");
 check(automaticCalls==1&&watchCalls==1,"game completion must not replay local actions");report(before);

 resetCase("busy_game_command_remains_serial");before=failures;hotkeys[11]='F'|0x200;registerKeys();shared->request=1;message(1011,'F');
 check(commands.empty()&&hotkeyPending.count==1,"game action must wait for command ACK");
 shared->ack=shared->request;advance(1);serviceHotkeyBatch();check(commands.size()==1&&commands[0]==AutoBoard,"queued game action recovers after ACK");report(before);
}
static void recoveryCases(){
 for(int gate=0;gate<4;++gate){
  const char*names[]={"local_disabled_recovery","local_focus_recovery","local_chat_recovery","local_pause_recovery"};
  resetCase(names[gate]);unsigned before=failures;hotkeys[0]='R'|0x200;registerKeys();
  if(gate==0)enabled=false;if(gate==1)focused=false;if(gate==2)incoming.chat=1;if(gate==3)incoming.commandStatus=activation::Paused;
  message(1000,'R');serviceHotkeyBatch();check(automaticCalls==0,"closed context must reject a local action");
  enabled=focused=true;incoming.chat=0;incoming.commandStatus=activation::Ready;registerKeys();advance(1);serviceHotkeyBatch();
  check(automaticCalls==0,"held input on recovery must not replay");
  release('R');message(1000,'R');check(automaticCalls==1,"fresh press after release must recover local input");report(before);
 }
 resetCase("local_old_epoch_pending_rejected");unsigned before=failures;hotkeys[0]='R'|0x200;registerKeys();
 check(hotkeyPending.add(*hotkeyRegistry.find(1000),now,commandMatch()),"stage an authenticated pending local input");
 incoming.match=snap.match=9;advance(1);serviceHotkeyBatch();check(automaticCalls==0&&hotkeyPending.count==0,"new match must cancel old pending local input");report(before);

 resetCase("local_expired_pending_rejected");before=failures;hotkeys[0]='R'|0x200;registerKeys();
 check(hotkeyPending.add(*hotkeyRegistry.find(1000),now,commandMatch()),"stage a local input before expiry");
 advance(2501);serviceHotkeyBatch();check(automaticCalls==0&&hotkeyPending.count==0,"local classification must not bypass original input age");report(before);

 resetCase("automatic_disabled_control_respected");before=failures;hotkeys[0]='R'|0x200;registerKeys();controlEnabled=false;message(1000,'R');
 check(automaticCalls==0&&!controlChecked[100]&&!notifications.empty(),"disabled automatic UI control must remain authoritative");report(before);

 resetCase("middle_preview_release_recovery");before=failures;hotkeys[11]='F'|0x200;registerKeys();
 shared->moOrdersPreview.active=1;shared->moOrdersPreview.match=8;message(1011,'F');check(commands.empty(),"current middle preview must retain ordinary command ownership");
 shared->moOrdersPreview.active=0;advance(1);serviceHotkeyBatch();check(commands.empty(),"preview release must not replay a held command");
 release('F');message(1011,'F');check(commands.size()==1,"fresh command recovers after middle preview ends");report(before);

 resetCase("old_middle_preview_does_not_own");before=failures;hotkeys[11]='F'|0x200;registerKeys();
 shared->moOrdersPreview.active=1;shared->moOrdersPreview.match=7;message(1011,'F');check(commands.size()==1,"old-match middle preview must not own current input");report(before);
}
static void typeCases(){
 for(int mode=0;mode<3;++mode){
  const char*names[]={"legacy_type_merge_double_tap","release_type_merge","explicit_double_type_merge"};
  resetCase(names[mode]);unsigned before=failures;
  WORD binding=WORD('T'|0x200|(mode==1?0x2000:mode==2?0x8000:0));
  typeBindings={{"A",binding},{"B",binding}};snap.typeCount=2;
  std::strcpy(snap.types[0].id,"A");std::strcpy(snap.types[1].id,"B");snap.types[0].kind=1;snap.types[1].kind=2;
  registerKeys();check(hotkeyRegistry.find(1100)&&!hotkeyRegistry.find(1101),"same type chord must register as one merged group");
  message(1100,'T');check(selectionCalls==(mode==0?1u:0u),"type gesture must wait for its actual configured phase");
  release('T');if(mode!=1){advance(100);message(1100,'T');}
  check(selectionCalls==(mode==0?2u:1u),"type gesture must dispatch exactly once per qualified input");
  check(selectionMask[0]&&selectionMask[1],"merged type selection must contain both configured types");
  check(selectionGlobal==(mode!=1),"double type selection upgrades global while release remains local");report(before);
 }
}
static void pendingCases(){
 resetCase("pending_local_bypass_preserves_fifo_and_wrap");unsigned before=failures;
 hotkeyroute::Pending p;hotkeyroute::Group g;g.actions={1011,1000,1730,1001};
 p.head=hotkeyroute::Pending::Capacity-2;check(p.add(g,0xfffffff0u,8),"wrapped queue accepts the whole group");
 hotkeyroute::Action a;
 check(p.popReady(0x20,8,a,localHotkeyAction)&&a.id==1000,"first local bypasses blocked command");
 check(p.popReady(0x20,8,a,localHotkeyAction)&&a.id==1730,"next local is consumed exactly once");
 check(!p.popReady(0x20,8,a,localHotkeyAction)&&p.count==2,"blocked game commands remain queued");
 check(p.popReady(0x20,8,a,[](int){return true;})&&a.id==1011&&a.tick==0xfffffff0u,"first deferred command retains identity and original age");
 check(p.popReady(0x20,8,a,[](int){return true;})&&a.id==1001&&p.count==0,"deferred commands retain FIFO across wrap");report(before);
}
static void formationFenceCases(){
 for(int gate=0;gate<3;++gate){
  const char*names[]={"formation_old_wm_after_reregister","formation_old_wm_before_context_resample","formation_old_wm_after_epoch_change"};
  resetCase(names[gate]);unsigned before=failures;
  extra.formationEnabled=true;extra.formationKeys[0]='F'|0x200;hotkeys[11]='F'|0x200;
  shared->formationPreview.active=1;shared->formationPreview.match=8;registerKeys();
  advance(1);messageAt=now;advance(1);
  if(gate<2){focused=false;serviceHotkeyBatch();if(gate==0)registerKeys();advance(2);focused=true;if(gate==0)registerKeys();}
  else{incoming.match=snap.match=shared->formationPreview.match=9;serviceHotkeyBatch();}
  handleHotkey(1011,LPARAM(('F'<<16)|MOD_CONTROL));
  check(shared->formationKey==0,"old context message must not become a new formation gesture even within 150ms");report(before);
 }
 resetCase("formation_owner_cancel_preserves_fresh_message");unsigned before=failures;
 extra.formationEnabled=true;extra.formationKeys[0]='F'|0x200;hotkeys[11]='F'|0x200;
 shared->formationPreview.active=1;shared->formationPreview.match=8;registerKeys();serviceHotkeyBatch();
 advance(10);messageAt=now;advance(5);serviceHotkeyBatch();handleHotkey(1011,LPARAM(('F'<<16)|MOD_CONTROL));
 check(shared->formationKey==1,"ordinary preview-owned sampler cancellation must not discard a valid shape message");report(before);

 resetCase("formation_context_timestamp_wrap");before=failures;now=lastData=0xfffffffbu;
 extra.formationEnabled=true;extra.formationKeys[0]='F'|0x200;hotkeys[11]='F'|0x200;
 shared->formationPreview.active=1;shared->formationPreview.match=8;registerKeys();advance(8);messageAt=now;
 handleHotkey(1011,LPARAM(('F'<<16)|MOD_CONTROL));check(shared->formationKey==1,"fresh formation context message must survive clock wrap");report(before);
}
int main(){formationCases();busyCases();recoveryCases();typeCases();pendingCases();formationFenceCases();std::cout<<"{\"checks\":"<<checks<<",\"scenarios\":"<<cases<<",\"failures\":"<<failures<<",\"passed\":"<<(failures?"false":"true")<<"}\n";return failures?1:0;}
'''


def load_fixture(root):
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('input_fixture', root / 'source/assist/tests/hotkey_command_context.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def program(root):
    f = load_fixture(root)
    main = (root / 'source/assist/source/main.cpp').read_text()
    hotkeys_path=root/'source/assist/source/hotkey_host.h'
    body = f.program(main, hotkeys_path.read_text())
    assert body.endswith(f.TESTS)
    body = body[:-len(f.TESTS)]
    body = body.replace('using HWND=uintptr_t;', 'using HWND=void*;')
    body = body.replace('static HWND ui=1;', 'static HWND ui=reinterpret_cast<HWND>(uintptr_t(1));')
    body = body.replace('static bool runtimeMiddleOwnsNow(){return middleOwns;}', 'static bool runtimeMiddleOwnsNow();')
    body = body.replace('static bool formationHotkey(WORD){return false;}', 'static bool formationHotkey(WORD);')
    body = body.replace('static bool automaticHotkey(WPARAM){return false;}', 'static bool automaticHotkey(WPARAM);')
    body = body.replace('static bool formationInputOwns(){return formationOwns;}', 'static bool formationInputOwns();')
    body = body.replace('static void sendSelection(int,bool,const uint8_t* =nullptr){throw std::runtime_error("unexpected selection route");}',
                        'static unsigned selectionCalls=0;static bool selectionGlobal=false;static uint8_t selectionMask[MaxTypes]{};\n'
                        'static void sendSelection(int,bool global,const uint8_t* mask=nullptr){++selectionCalls;selectionGlobal=global;if(mask)std::memcpy(selectionMask,mask,MaxTypes);}')
    body = body.replace('namespace watchhost {static void pin(uint32_t){}}', 'namespace watchhost {static void pin(uint32_t){++watchCalls;}}')
    marker = 'namespace watchhost {'
    pos = body.index(marker)
    body = body[:pos] + EXTRA + '\n' + body[pos:]
    middle = (root / 'source/assist/source/middle_input_host.h').read_text()
    formation = (root / 'source/assist/source/formation_host.h').read_text()
    automatic = (root / 'source/assist/source/automatic_hotkeys_host.h').read_text()
    body += '\n#include "middle_input_owner.h"\n'
    body += '\n#include "input_ownership.h"\n' + f.function(main,'formationInputOwns') + '\n'
    for name in ('runtimeGesturePolicy','runtimeGestureEnabled','runtimeGestureReserved','runtimeGestureKeyboard','runtimeMiddleOwnsNow'):
        body += f.function(middle, name) + '\n'
    body += f.function(formation, 'formationHotkey') + '\n'
    body += f.function(automatic, 'automaticHotkey') + '\n'
    return body + TESTS


def run(root, out):
    root=root.resolve();out.mkdir(parents=True,exist_ok=True)
    paths=['source/assist/source/'+p for p in ['main.cpp','hotkey_host.h','hotkey_routes.h','middle_input_host.h','middle_input_owner.h','formation_host.h','automatic_hotkeys_host.h','automatic_features.h','input_ownership.h','control_link.h','read_only_lifecycle.h','protocol.h','additional_settings.h','type_hotkey.h','mo_orders.h']]+['source/shared/source/key_gesture.h','source/assist/tests/hotkey_command_context.py','source/assist/tests/hotkey_entry_recovery.py']
    before={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths}
    cpp=out/'production.cpp';cpp.write_text(program(root))
    exe=out/'production';dep=out/'production.d'
    cmd=['g++','-std=c++17','-O2','-UNDEBUG','-MMD','-MF',str(dep),'-I',str(root/'source/assist/source'),str(cpp),'-o',str(exe)]
    built=subprocess.run(cmd,capture_output=True,text=True);(out/'compile.log').write_text(built.stdout+built.stderr)
    if built.returncode:raise RuntimeError(built.stderr)
    ran=subprocess.run([str(exe)],capture_output=True,text=True);(out/'run.log').write_text(ran.stdout+ran.stderr)
    lines=[json.loads(line) for line in ran.stdout.splitlines()]
    dependencies=dep.read_text().replace('\\\n',' ').split(':',1)[1].split()
    after={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths}
    if before!=after:raise RuntimeError('production source changed while this input fixture ran')
    result={'scope':__doc__,'passed':ran.returncode==0 and lines[-1]['passed'],'source_sha256':before,'compile_command':cmd,'compile_exit':built.returncode,'run_exit':ran.returncode,'cases':lines[:-1],'summary':lines[-1],'stdout':ran.stdout,'stderr':ran.stderr,'generated_cpp_sha256':hashlib.sha256(cpp.read_bytes()).hexdigest(),'run_log_sha256':hashlib.sha256((out/'run.log').read_bytes()).hexdigest(),'production_header_sha256':{str(Path(p).resolve()):hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in dependencies if Path(p).resolve().is_relative_to(root)}}
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root',type=Path,default=ROOT)
    p.add_argument('--out',type=Path)
    args=p.parse_args()
    if args.out:
        result=run(args.source_root,args.out.resolve())
    else:
        import tempfile
        with tempfile.TemporaryDirectory(prefix='hotkey-entry-') as directory:
            result=run(args.source_root,Path(directory))
    print(json.dumps(result['summary']))
    return 0 if result['passed'] else 1

if __name__=='__main__':raise SystemExit(main())
