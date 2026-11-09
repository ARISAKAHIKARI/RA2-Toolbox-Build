"""Run production Host input diagnostics and admission with controlled Win32 seams.

This does not deliver Windows keyboard messages or execute game commands. It
checks that the automatic evidence distinguishes the actual Host return paths
while preserving the existing command dispatch and held-key cancellation.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess

SCOPE = "production Host diagnostic and hotkey functions with controlled Win32/input/command boundaries; no Windows/game execution"

EXTRA = r'''
static HWND game=500,currentForeground=500;
static DWORD pid=53692,foregroundPID=53692;
static bool validWindow=true,iconicWindow=false;
static HWND GetForegroundWindow(){return currentForeground;}
static bool IsWindow(HWND w){return w&&validWindow;}
static bool IsIconic(HWND w){return w==game&&iconicWindow;}
static DWORD GetWindowThreadProcessId(HWND w,DWORD*p){*p=w==game?pid:foregroundPID;return *p?75764:0;}
static DWORD sampleClock(Shared*){return now;}
static std::vector<std::wstring> logs;
static void critical(const std::wstring&line){logs.push_back(line);}
static hotkeydiagnostic::Monitor hotkeyInputMonitor;
static uint32_t hotkeyBindingRevision=0;
'''

CASES = r'''
static unsigned checks=0,scenarios=0;
static void check(bool value,const char*label){++checks;if(!value)throw std::runtime_error(label);}
static bool logged(const wchar_t*part){for(const auto&line:logs)if(line.find(part)!=std::wstring::npos)return true;return false;}
static void setup(){
 ++scenarios;now+=10000;lastData=messageAt=now;shared=storage.get();shared->stop=0;
 shared->request=shared->ack=shared->selectRequest=shared->selectAck=0;
 shared->formationPreview.active=0;shared->formationPreview.match=8;shared->formationConsumedUntil=0;
 shared->moOrdersPreview.active=0;shared->moOrdersPreview.match=8;
 incoming={};incoming.status=incoming.commandStatus=1;incoming.match=8;snap=incoming;snap.readStatus=snap.renderStatus=1;
 enabled=focused=validWindow=true;iconicWindow=middleOwns=formationOwns=hasSelection=false;
 game=currentForeground=500;pid=foregroundPID=53692;registeredHotkeyMode=0;
 osInstallOkay=true;hookSmartUnload=hookAutoCS=buildAnywhereMode=false;
 extra={};extra.stickyKey=extra.variantKey=0;for(auto&k:extra.missionModeKeys)k=0;for(auto&k:extra.watchKeys)k=0;
 for(auto&k:hotkeys)k=0;hotkeys[11]='F'|0x200;for(auto&k:socialKeys)k=0;
 ejectKey=unloadKey=csKey=idleAircraftKey=0;swBindings.clear();typeBindings.clear();
 commands.clear();notifications.clear();logs.clear();osRegistered.clear();registeredKeyIDs.clear();
 hotkeyRegistry.clear();hotkeyPending.clear();typeTap.clear();variantTap.clear();formationTap.clear();
 hotkeyInputMonitor={};hotkeyDiagnostics={};commandQueue.size=0;std::memset(rawKeys,0,sizeof rawKeys);
 registerKeys();serviceHotkeyBatch();
 check(registeredHotkeyMode==2&&hotkeyRegistry.find(1011),"baseline binding exists");
}
static void advance(unsigned ms=10){now+=ms;lastData=now;messageAt=now;}
static void press(bool ctrl=true){advance();rawKeys[17]=ctrl;rawKeys['F']=1;serviceHotkeyBatch();}
static void release(){advance();rawKeys['F']=0;serviceHotkeyBatch();}
static void pass(const char*name){std::cout<<"PASS hotkey_live_observation::"<<name<<"\n";}
int main(){try{
 setup();press();
 check(commands.size()==1&&commands.back()==AutoBoard,"observed physical edge still dispatches AutoBoard");
 check(hotkeyInputMonitor.configuredPresses==1&&hotkeyInputMonitor.chordMatches==1,"physical and chord counters");
 check(logged(L"hotkey-physical kind=press")&&logged(L"actualModifiers=2 modifiersMatch=1 gate=ready"),"physical evidence is automatic");
 check(logged(L"stage=dispatch")&&logged(L"stage=execute"),"dispatch and execution automatic");pass("ordinary configured edge");

 setup();press(false);
 check(commands.empty(),"wrong modifiers do not dispatch");check(hotkeyInputMonitor.mismatches==1,"wrong modifiers counted");
 check(logged(L"actualModifiers=0 modifiersMatch=0"),"wrong modifier evidence");pass("modifier mismatch");

 setup();currentForeground=600;foregroundPID=99;advance();serviceHotkeyBatch();rawKeys[17]=rawKeys['F']=1;advance();serviceHotkeyBatch();
 check(hotkeyInputMonitor.gate==hotkeydiagnostic::Gate::NotForeground,"foreign focus gate");
 check(hotkeyInputMonitor.reads==1&&commands.empty(),"foreign focus does not observe or dispatch keys");
 check(logged(L"foregroundPid=99")&&logged(L"foregroundHwnd=600"),"actual foreground identity logged");pass("foreign foreground");

 setup();currentForeground=600;foregroundPID=pid;iconicWindow=true;advance();serviceHotkeyBatch();
 check(hotkeyInputMonitor.gate==hotkeydiagnostic::Gate::NotForeground,"existing iconic native gate preserved");
 check(logged(L"gameWindowIconic=1")&&logged(L"foregroundPid=53692"),"iconic native and actual same-pid foreground distinguished");
 check(commands.empty(),"diagnostic does not widen focus authorization");pass("same pid different foreground and iconic native");

 setup();incoming.chat=1;advance();serviceHotkeyBatch();press();
 check(hotkeyInputMonitor.gate==hotkeydiagnostic::Gate::Chat,"chat gate");check(hotkeyInputMonitor.reads==1&&commands.empty(),"chat has no added physical reads");
 incoming.chat=0;advance();serviceHotkeyBatch();
 check(logged(L"kind=held-on-sync")&&hotkeyInputMonitor.heldOnSync==1,"resume-held input marked as baseline");
 check(commands.empty(),"held-on-resume is not a command");release();press();check(commands.size()==1,"fresh press after release works");pass("chat and held resumption");

 setup();incoming.commandStatus=10;advance();serviceHotkeyBatch();press();
 check(hotkeyInputMonitor.gate==hotkeydiagnostic::Gate::ConnectionNotReady,"runtime readiness distinguished");
 check(hotkeyInputMonitor.reads==1&&commands.empty(),"paused context not read or dispatched");
 check(logged(L"hotkeyGate=connection-not-ready"),"readiness evidence");pass("runtime pause gate");

 setup();now+=1600;serviceHotkeyBatch();
 check(hotkeyInputMonitor.gate==hotkeydiagnostic::Gate::Stale,"staleness distinguished from readiness");
 check(hotkeyInputMonitor.reads==1&&commands.empty(),"stale sample does not read or dispatch");
 check(logged(L"hotkeyGate=stale"),"staleness logged");pass("stale bridge");

 setup();shared->formationPreview.active=1;advance();serviceHotkeyBatch();press();
 check(hotkeyInputMonitor.gate==hotkeydiagnostic::Gate::FormationOwned,"formation owner gate");
 check(hotkeyInputMonitor.configuredPresses==1&&commands.empty(),"configured edge observed while ownership still blocks dispatch");
 check(logged(L"gate=formation-owned")&&logged(L"formationActive=1"),"formation reason and state recorded");pass("formation ownership");

 setup();middleOwns=true;shared->moOrdersPreview.active=1;advance();serviceHotkeyBatch();press();
 check(hotkeyInputMonitor.gate==hotkeydiagnostic::Gate::MiddleOwned,"middle owner gate");
 check(hotkeyInputMonitor.configuredPresses==1&&commands.empty(),"middle ownership observation does not authorize");
 check(logged(L"gate=middle-owned")&&logged(L"middleActive=1"),"middle state recorded");pass("middle ownership");

 setup();rawKeys['Z']=1;advance();serviceHotkeyBatch();
 check(hotkeyInputMonitor.configuredPresses==0&&hotkeyInputMonitor.heldOnSync==0,"unconfigured key not recorded");
 check(commands.empty()&&!logged(L"hotkey-physical"),"unconfigured input leaves no event log");
 check(hotkeyInputMonitor.reads==2,"reads still accurately count observation");pass("unconfigured key privacy");

 setup();osInstallOkay=false;registerKeys();advance();serviceHotkeyBatch();press();
 check(hotkeyDiagnostics.fallbacks==1&&hotkeyRegistry.find(1011),"OS conflict retains registry binding");
 check(commands.size()==1&&commands.back()==AutoBoard,"conflict fallback dispatches once");
 check(logged(L"foreground-poll Win32=1409"),"registration error remains automatic");pass("registration conflict fallback");

 setup();advance();rawKeys[17]=rawKeys['F']=1;handleHotkey(1011,LPARAM(('F'<<16)|MOD_CONTROL));serviceHotkeyBatch();
 check(hotkeyDiagnostics.messages==1&&commands.size()==1,"OS and sampled route share exactly one dispatch");
 check(logged(L"stage=message")&&logged(L"stage=dispatch"),"message and dispatch evidence automatic");
 check(hotkeyInputMonitor.configuredPresses==1,"physical diagnostic is independent from deduplicated command");pass("OS message and poll deduplication");

 setup();const auto count=logs.size();for(unsigned n=0;n<10;++n){advance(10);serviceHotkeyBatch();}
 check(logs.size()==count,"steady service does not spam logs");
 now+=5000;lastData=now;serviceHotkeyBatch();check(logs.size()==count+1,"active service heartbeat after five seconds");
 check(hotkeyInputMonitor.calls==12,"service call count survives heartbeat suppression");pass("bounded heartbeat");

 setup();for(unsigned n=0;n<100;++n)logHotkeyInput(L"queue",1011);
 check(hotkeyDiagnostics.traceDropped>0,"trace limit records dropped diagnostic lines");
 check(commands.empty(),"diagnostic calls never create commands");
 advance(1001);const auto countAfter=logs.size();logHotkeyInput(L"queue",1011);check(logs.size()==countAfter+1,"trace allowance resumes after interval");pass("bounded event logging");

 std::cout<<"{\"suite\":\"hotkey_live_observation\",\"passed\":true,\"scenarios\":"<<scenarios<<",\"checks\":"<<checks<<",\"scope\":\"production Host diagnostic and hotkey functions with controlled Win32/input/command boundaries; no Windows/game execution\"}\n";
 return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
'''


def run(root: Path, out: Path) -> dict:
    driver = root / 'source/assist/tests/hotkey_command_context.py'
    spec = importlib.util.spec_from_file_location('current_hotkey_context', driver)
    context = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(context)
    paths = [f'source/assist/source/{x}' for x in (
        'main.cpp', 'hotkey_host.h', 'hotkey_input_diagnostic.h', 'input_ownership.h',
        'hotkey_routes.h', 'control_link.h', 'protocol.h', 'read_only_lifecycle.h', 'additional_settings.h',
        'type_hotkey.h', 'automatic_features.h', 'activation_diagnostic.h')]
    paths += ['source/shared/source/key_gesture.h', 'source/assist/tests/hotkey_command_context.py',
              'source/assist/tests/hotkey_live_observation.py']
    sources = {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in paths}
    main = (root / paths[0]).read_text()
    host = (root / paths[1]).read_text()
    preamble = context.PREAMBLE
    for line in (
        'static bool foregroundGame(){return focused;}',
        'static bool formationInputOwns(){return formationOwns;}',
        'static void critical(const std::wstring&){}',
        'static void logHotkeyInput(const wchar_t*,int=0,uint16_t=0,DWORD=0){}',
        'static void logHotkeyBindings(){}',
        'static void traceHotkeyService(bool,bool,bool,bool){}',
        'static void traceHotkeyKeys(const uint8_t*){}',
    ):
        if preamble.count(line) != 1:
            raise RuntimeError('Fixture seam changed: ' + line)
        preamble = preamble.replace(line, '', 1)
    old = re.search(r'struct HotkeyDiagnostics\{[^}]*\};', preamble)
    actual = re.search(r'struct HotkeyDiagnostics \{[^}]*\};', main)
    if not old or not actual:
        raise RuntimeError('Diagnostic structure not found')
    preamble = preamble.replace(old.group(), actual.group())
    preamble = '#include "hotkey_input_diagnostic.h"\n#include "input_ownership.h"\n' + preamble + EXTRA
    names = ('commandLink', 'commandMatch', 'commandChannelReady', 'commandFresh', 'foregroundGame',
             'formationInputOwns', 'commandHotkeyContext', 'desiredHotkeyMode', 'modifiers',
             'hotkeyDiagnosticState', 'hotkeyTraceAllowed', 'traceHotkeyService', 'traceHotkeyKeys',
             'logHotkeyInput', 'logHotkeyBindings', 'synchronizeHotkeyKeys', 'registerKey', 'registerKeys')
    parts = [context.function(main, name) for name in names]
    parts += [context.function(host, name) for name in ('ownGroundHotkey', 'dispatchHotkeyGroup', 'localHotkeyAction',
              'executeHotkey', 'serviceHotkeyBatch', 'handleHotkey')]
    actions = re.search(r'static const int actions\[\]\s*=\s*\{[^}]*\};', main).group()
    cpp = out / 'production.cpp'
    cpp.write_text(preamble + '\n' + actions + '\n' + '\n'.join(parts) + CASES)
    command = ['g++', '-std=c++17', '-O2', '-UNDEBUG', '-MMD', '-MF', str(out / 'production.d'),
               '-I', str(root / 'source/assist/source'), str(cpp), '-o', str(out / 'production')]
    built = subprocess.run(command, capture_output=True, text=True)
    (out / 'compile.log').write_text(built.stdout + built.stderr)
    if built.returncode:
        raise RuntimeError('Compilation failed: ' + built.stderr)
    executed = subprocess.run([str(out / 'production')], capture_output=True, text=True)
    (out / 'run.log').write_text(executed.stdout + executed.stderr)
    if executed.returncode:
        raise RuntimeError('Execution failed: ' + executed.stdout + executed.stderr)
    summary = json.loads(executed.stdout.strip().splitlines()[-1])
    if summary['passed'] is not True or summary['scope'] != SCOPE:
        raise RuntimeError('Invalid execution summary')
    for p, digest in sources.items():
        if hashlib.sha256((root / p).read_bytes()).hexdigest() != digest:
            raise RuntimeError('Source changed during run: ' + p)
    result = {**summary, 'sources': sources, 'compile_command': command,
              'compile_exit_code': built.returncode, 'run_exit_code': executed.returncode,
              'windows_executed': False, 'game_tested': False}
    (out / 'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    print(json.dumps(run(args.source_root.resolve(), args.out.resolve()), ensure_ascii=False))
