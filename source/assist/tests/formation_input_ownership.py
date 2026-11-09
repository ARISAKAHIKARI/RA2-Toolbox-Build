#!/usr/bin/env python3
"""Actual Host input routes with controlled key/command boundaries.

The current formationInputOwns, registration, Registry, dispatch and sampler
functions are compiled from production. The negative control replaces only
formationInputOwns with its explicit former expression; it is not a rebuilt
historical product or a Windows/game execution. It demonstrates two ownership
defects, without identifying either as the cause of the user's recorded input
failure. Host GetAsyncKeyState and WM_HOTKEY delivery remain controlled seams.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

BASELINE_OWNER = '''static bool formationInputOwns(){return shared&&(shared->formationPreview.active||int32_t(shared->formationConsumedUntil-sampleClock(shared))>0);}'''

TESTS = r'''
static unsigned checks=0,failures=0,scenarios=0;
static void verify(bool okay,const char*name){++checks;if(!okay){++failures;std::cerr<<"FAIL "<<name<<"\n";}}
static void resetCase(uint32_t at,uint32_t deadline,bool active,uint32_t previewMatch){
 ++scenarios;now=lastData=messageAt=at;
 shared=storage.get();shared->stop=0;shared->request=shared->ack=0;
 shared->selectRequest=shared->selectAck=0;
 shared->formationPreview={};shared->formationPreview.active=active;shared->formationPreview.match=previewMatch;
 shared->formationConsumedUntil=deadline;shared->formationPolicy.enabled=0;
 incoming={};incoming.status=incoming.bridgeStatus=incoming.commandStatus=1;incoming.match=8;
 snap=incoming;snap.renderStatus=snap.readStatus=1;
 enabled=focused=true;hasSelection=keysRegistered=false;registeredHotkeyMode=0;
 formationOwns=middleOwns=false;osInstallOkay=true;
 hookSmartUnload=hookAutoCS=buildAnywhereMode=false;
 extra={};extra.stickyKey=extra.variantKey=0;extra.formationEnabled=false;
 for(auto&key:extra.missionModeKeys)key=0;
 for(auto&key:extra.watchKeys)key=0;
 std::memset(hotkeys,0,sizeof hotkeys);hotkeys[11]=WORD('F'|0x200);
 std::memset(socialKeys,0,sizeof socialKeys);ejectKey=unloadKey=csKey=idleAircraftKey=0;
 std::memset(rawKeys,0,sizeof rawKeys);
 commandQueue.size=0;typeBindings.clear();swBindings.clear();notifications.clear();commands.clear();
 registeredKeyIDs.clear();osRegistered.clear();hotkeyRegistry.clear();hotkeyPending.clear();
 typeTap.clear();variantTap.clear();formationTap.clear();hotkeyDiagnostics={};
}
static void scenario(const char*name,uint32_t at,uint32_t deadline,bool active,uint32_t previewMatch,bool wm,bool expectedSend){
 resetCase(at,deadline,active,previewMatch);const unsigned before=failures;
 registerKeys();
 verify(commandChannelReady(),"command channel remains healthy");
 verify(registeredHotkeyMode==2,"command registration remains admitted");
 verify(hotkeyRegistry.find(1011)!=nullptr,"real AutoBoard Ctrl+F is registered");
 ++now;lastData=now;rawKeys[17]=rawKeys['F']=1;
 if(wm){messageAt=now;handleHotkey(1011,LPARAM(('F'<<16)|MOD_CONTROL));}
 else serviceHotkeyBatch();
 verify((commands.size()==1)==expectedSend,name);
 verify(commands.empty()||(commands.size()==1&&commands[0]==AutoBoard),"only the actual AutoBoard action reaches the sink");
 verify(hotkeyDiagnostics.messages==(wm?1u:0u),"WM receipt remains distinct from physical sampling");
 std::cout<<"{\"scenario\":\""<<name<<"\",\"expected_send\":"<<expectedSend<<",\"sent\":"<<commands.size()
 <<",\"owned\":"<<formationInputOwns()<<",\"mode\":"<<registeredHotkeyMode<<",\"messages\":"<<hotkeyDiagnostics.messages
 <<",\"sampled\":"<<hotkeyDiagnostics.sampled<<",\"actions\":"<<hotkeyDiagnostics.actions<<",\"blocked\":"<<hotkeyDiagnostics.blocked
 <<",\"passed\":"<<(failures==before?"true":"false")<<"}\n";
}
int main(){
 using assist::inputownership::afterGesture;
 scenario("low_uptime_poll",10000,0,false,8,false,true);
 scenario("unset_high_uptime_poll",0x90000000u,0,false,8,false,true);
 scenario("old_match_active_preview_poll",10000,0,true,7,false,true);
 scenario("old_match_lease_poll",10000,afterGesture(10000),false,7,false,true);
 scenario("unset_high_uptime_wm",0x90000000u,0,false,8,true,true);
 scenario("current_active_preview_poll",10000,0,true,8,false,false);
 scenario("current_commit_lease_poll",10000,afterGesture(10000),false,8,false,false);
 scenario("current_expired_lease_poll",10000,9999,false,8,false,true);
 scenario("current_wrap_lease_poll",0xffffff80u,afterGesture(0xffffff80u),false,8,false,false);
 scenario("current_normalized_zero_lease_poll",0xffffff38u,afterGesture(0xffffff38u),false,8,false,false);
 scenario("normalized_zero_lease_expired_poll",1,1,false,8,false,true);
 scenario("ancient_deadline_poll",0x90000000u,1000,false,8,false,true);
 scenario("current_active_preview_wm",10000,0,true,8,true,false);
 std::cout<<"{\"passed\":"<<(failures?"false":"true")<<",\"scenarios\":"<<scenarios
 <<",\"checks\":"<<checks<<",\"failures\":"<<failures<<"}\n";
 return failures?1:0;
}
'''


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_fixture(path: Path):
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('formation_host_fixture', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def make_program(fixture, main: str, hotkeys: str, baseline: bool) -> str:
    program = fixture.program(main, hotkeys)
    assert program.endswith(fixture.TESTS)
    program = program[:-len(fixture.TESTS)]
    old_stub = 'static bool formationInputOwns(){return formationOwns;}'
    assert program.count(old_stub) == 1, 'Host fixture owner seam changed'
    owner = BASELINE_OWNER if baseline else fixture.function(main, 'formationInputOwns')
    replacement = ('#include "input_ownership.h"\n'
                   'static uint32_t sampleClock(const Shared*){return now;}\n'
                   'static uint32_t commandMatch();\n' + owner)
    program = program.replace(old_stub, replacement)
    # These are observational seams only; preserve every production admission
    # and dispatch decision in serviceHotkeyBatch and handleHotkey.
    seams = ''
    if 'static void traceHotkeyService(' not in program:
        seams += 'static void traceHotkeyService(bool,bool,bool,bool){}\n'
    if 'static void traceHotkeyKeys(' not in program:
        seams += 'static void traceHotkeyKeys(const uint8_t*){}\n'
    marker = 'static DWORD GetTickCount()'
    position = program.index(marker)
    program = program[:position] + seams + program[position:]
    return program + TESTS


def execute(root: Path, work: Path, fixture, main: str, hotkeys: str, baseline: bool) -> dict:
    name = 'baseline-expression' if baseline else 'current'
    directory = work / name
    directory.mkdir(parents=True, exist_ok=True)
    cpp = directory / 'production.cpp'
    cpp.write_text(make_program(fixture, main, hotkeys, baseline))
    executable = directory / 'production'
    depfile = directory / 'dependencies.d'
    command = ['g++', '-std=c++17', '-O2', '-UNDEBUG', '-MMD', '-MF', str(depfile),
               '-I', str(root / 'source/assist/source'), str(cpp), '-o', str(executable)]
    built = subprocess.run(command, capture_output=True, text=True)
    (directory / 'compile.log').write_text(built.stdout + built.stderr)
    if built.returncode:
        raise RuntimeError(f'{name} compile failed: {built.stderr}')
    ran = subprocess.run([str(executable)], capture_output=True, text=True)
    (directory / 'run.log').write_text(ran.stdout + ran.stderr)
    records = [json.loads(line) for line in ran.stdout.splitlines()]
    dependencies = depfile.read_text().replace('\\\n', ' ').split(':', 1)[1].split()
    return {'compile_command': command, 'compile_exit': built.returncode,
            'run_exit': ran.returncode, 'cases': records[:-1], 'summary': records[-1],
            'stdout': ran.stdout, 'stderr': ran.stderr, 'directory': str(directory),
            'generated_cpp_sha256': sha(cpp), 'run_log_sha256': sha(directory / 'run.log'),
            'production_header_sha256': {str(Path(p).resolve()): sha(Path(p)) for p in dependencies
                                       if Path(p).resolve().is_relative_to(root)}}


def run(root: Path, work: Path) -> dict:
    files = [
        'source/assist/source/main.cpp', 'source/assist/source/hotkey_host.h',
        'source/assist/source/input_ownership.h', 'source/assist/source/formation_bridge.h',
        'source/assist/source/hotkey_routes.h', 'source/assist/source/control_link.h',
        'source/assist/source/protocol.h', 'source/shared/source/key_gesture.h',
        'source/assist/tests/hotkey_command_context.py', 'source/assist/tests/formation_input_ownership.py',
    ]
    before = {path: sha(root / path) for path in files}
    fixture = load_fixture(root / 'source/assist/tests/hotkey_command_context.py')
    main = (root / files[0]).read_text()
    hotkeys = (root / files[1]).read_text()
    assert 'inputownership::ownsFormation' in fixture.function(main, 'formationInputOwns')
    producer = (root / 'source/assist/source/formation_bridge.h').read_text()
    assert producer.count('formationConsumedUntil=inputownership::afterGesture(') == 6
    current = execute(root, work, fixture, main, hotkeys, False)
    baseline = execute(root, work, fixture, main, hotkeys, True)
    after = {path: sha(root / path) for path in files}
    assert before == after, 'production sources changed while the fixture ran'
    expected_failures = {'unset_high_uptime_poll', 'old_match_active_preview_poll',
                         'old_match_lease_poll', 'unset_high_uptime_wm', 'ancient_deadline_poll'}
    baseline_failures = {case['scenario'] for case in baseline['cases'] if not case['passed']}
    passed = (current['run_exit'] == 0 and current['summary']['passed'] and
              current['summary']['checks'] == 78 and current['summary']['scenarios'] == 13 and
              baseline['run_exit'] == 1 and not baseline['summary']['passed'] and
              baseline['summary']['failures'] == 5 and baseline_failures == expected_failures)
    return {'passed': bool(passed), 'scope': __doc__, 'source_sha256': before,
            'baseline_kind': 'explicit former owner expression only; current Host and headers otherwise identical',
            'baseline_expression': BASELINE_OWNER, 'case_count': 13, 'check_count': 78,
            'expected_baseline_failures': sorted(expected_failures),
            'current': current, 'baseline': baseline}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    root = args.source_root.resolve()
    if args.output:
        output = args.output.resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        work = output.parent / (output.stem + '-artifacts')
        result = run(root, work)
        output.write_text(json.dumps(result, indent=2) + '\n')
    else:
        with tempfile.TemporaryDirectory(prefix='formation-ownership-') as temporary:
            result = run(root, Path(temporary))
    print(json.dumps({key: result[key] for key in ('passed', 'case_count', 'check_count')} |
                     {'baseline_failures': result['baseline']['summary']['failures']}))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
