"""Compare the production cold-start flow with its frozen pre.4 demand call site.

Complete Host sampling, merge, identity and registration functions are used.
Only Win32/RPM/IPC boundaries are simulated. The negative control changes the
three original tick statements, not the input data or sampling implementation.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess

BASELINE = 'source/assist/tests/fixtures/hotkey_pre1474/bridge_callsite.cpp'
BASELINE_SHA = '5cf66cfe242d5d65bf9ea2f468c1a07a0526d9a7de90964e1132e9a9f4a526c2'
BOUNDARY = 'source/research/evidence/pre147-hotkey-origin/bootstrap/cold_bootstrap.py'
SCOPE = 'complete production Host sample/merge/registration functions and tick demand statements with controlled Win32/RPM/IPC; no Windows/game execution'

TESTS = r'''
static unsigned checks=0,failures=0,caseFailures=0;
static void check(bool value,const char*label){++checks;if(!value){++failures;++caseFailures;std::cout<<"CHECK_FAILED "<<label<<"\n";}}
static void reset(const std::string&scenario){
 now+=20000;lastData=0;shared=nullptr;snap={};incoming={};externalPublished={};externalData=nullptr;
 externalAt=0;externalSourceConfirmed=false;externalWorldSource.reset();externalSampler.reset();
 bridgeAttempts=samplerCalls=scopeAttempts=scopeFailures=wakes=0;failedConnections=0;worldAvailable=true;
 readerValid=readerReadable=true;configuredFlags=0;enabled=focused=true;
 debugPageActive=techMode=buildTimeTooltipEnabled=hasSelection=false;commandQueue.size=0;
 extra={};extra.missionModeKeys[selectedmode::OwnGround]=0;
 for(auto&key:hotkeys)key=0;hotkeys[11]='F'|0x200;
 keysRegistered=false;registeredHotkeyMode=0;registeredKeyIDs.clear();osRegistered.clear();hotkeyRegistry.clear();hotkeyPending.clear();
 if(scenario=="default_own_ground")extra=AdditionalSettings{};
 if(scenario=="legacy_advanced_settings")configuredFlags=AdvancedInput|NativeOrders;
 if(scenario=="auto_repair")configuredFlags=AutoRepair;
 if(scenario=="already_connected"||scenario=="delayed_world_publication")ensureBridge();
 if(scenario=="missing_reader")readerValid=readerReadable=false;
 if(scenario=="query_only_reader")readerReadable=false;
 if(scenario=="disabled_host")enabled=false;
 if(scenario=="delayed_connection")failedConnections=2;
 if(scenario=="delayed_world_publication")worldAvailable=false;
}
static void publishCommandBoundary(){
 if(!shared)return;incoming.status=incoming.bridgeStatus=incoming.commandStatus=1;
 incoming.match=1;incoming.chat=0;lastData=now;snap=incoming;mergeExternal();
}
int main(){
 unsigned scenarios=0;
 for(const std::string scenario:{"manual_only","default_own_ground","legacy_advanced_settings","auto_repair","already_connected","manual_restart","missing_reader","query_only_reader","disabled_host","delayed_connection","delayed_world_publication"}){
  ++scenarios;caseFailures=0;reset(scenario);
  const bool readyExpected=scenario!="missing_reader"&&scenario!="query_only_reader"&&scenario!="disabled_host";
  const bool featureExpected=scenario=="default_own_ground"||scenario=="legacy_advanced_settings"||scenario=="auto_repair";
  check(runtimeFeatureDemand(flags(),extra,techMode,buildTimeTooltipEnabled)==featureExpected,"independent legacy feature demand");
  for(unsigned iteration=0;iteration<12;++iteration){
   now+=100;if(scenario=="delayed_world_publication"&&iteration==3)worldAvailable=true;
   oneTick();publishCommandBoundary();
  }
  const bool registered=std::find(osRegistered.begin(),osRegistered.end(),1011)!=osRegistered.end();
  check((shared!=nullptr)==readyExpected,"Runtime bootstraps without a published world");
  check((externalData!=nullptr)==readyExpected,"world consumer receives publication");
  check((registeredHotkeyMode==2)==readyExpected,"command hotkey mode becomes ready");
  check(registered==readyExpected,"real AutoBoard binding registers");
  check((samplerCalls>0)==readyExpected,"complete sampler executes");
  if(!readyExpected)check(bridgeAttempts==0,"no bootstrap for disabled/unreadable session");
  else if(scenario=="delayed_connection")check(bridgeAttempts==3,"failed producer setup retries without cached samples");
  else check(bridgeAttempts==1,"healthy connection is not installed repeatedly");
  std::cout<<"{\"case\":\""<<scenario<<"\",\"passed\":"<<(caseFailures?"false":"true")<<",\"failures\":"<<caseFailures
   <<",\"feature_demand\":"<<runtimeFeatureDemand(flags(),extra,techMode,buildTimeTooltipEnabled)
   <<",\"bridge_attempts\":"<<bridgeAttempts<<",\"world_scope_failures\":"<<scopeFailures
   <<",\"sampler_calls\":"<<samplerCalls<<",\"mode\":"<<registeredHotkeyMode<<",\"autoboard_registered\":"<<registered<<"}\n";
 }
 std::cout<<"{\"suite\":\"hotkey_cold_start\",\"passed\":"<<(failures?"false":"true")
  <<",\"scenarios\":"<<scenarios<<",\"checks\":"<<checks<<",\"failures\":"<<failures<<"}\n";
 return failures?1:0;
}
'''


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(root: Path, out: Path) -> dict:
    spec = importlib.util.spec_from_file_location('historical_cold_boundary', root / BOUNDARY)
    boundary = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(boundary)
    if digest(root / BASELINE) != BASELINE_SHA:
        raise RuntimeError('Frozen pre.4 demand statements changed')
    main_path = 'source/assist/source/main.cpp'
    main = (root / main_path).read_text()
    found = re.search(r'sampleExternal\(now\);uint32_t configured=flags\(\);\n bool needsBridge=[^\n]+\n if\(needsBridge&&!shared\)ensureBridge\(\);', main)
    if not found:
        raise RuntimeError('Complete production demand call site not found')
    registration = 'if(registeredHotkeyMode!=desiredHotkeyMode())registerKeys();'
    registration_at = main.find(registration, main.index('static void tick()'))
    if registration_at < 0 or registration_at > found.start():
        raise RuntimeError('Production registration order changed; update the fixture deliberately')
    names = ('commandLink', 'commandMatch', 'displayMatch', 'commandChannelReady', 'commandHotkeyContext',
             'desiredHotkeyMode', 'modifiers', 'synchronizeHotkeyKeys', 'registerKey', 'registerKeys',
             'discardExternalWorld', 'mergeExternal', 'sampleExternal')
    functions = '\n'.join(boundary.function(main, name) for name in names)
    preamble = boundary.PREAMBLE
    preamble = preamble.replace('static bool enabled=true,focused=true,diagEnabled=false,infoAdaptive=false;',
        'static bool enabled=true,focused=true,diagEnabled=false,infoAdaptive=false;\nstatic bool readerValid=true,readerReadable=true,worldAvailable=true;static unsigned failedConnections=0;')
    preamble = preamble.replace('++bridgeAttempts;shared=storage.get();', '++bridgeAttempts;if(failedConnections){--failedConnections;return false;}shared=storage.get();')
    preamble = preamble.replace('bool valid()const{return true;}', 'bool valid()const{return readerValid;}bool canRead()const{return readerValid&&readerReadable;}')
    preamble = preamble.replace('bound(shared!=nullptr)', 'bound(shared!=nullptr&&readerReadable&&worldAvailable)')
    preamble += '\nstatic void logHotkeyBindings(){}\n'
    paths = [main_path, BASELINE, BOUNDARY, 'source/assist/tests/hotkey_cold_start.py']
    paths += [f'source/assist/source/{name}' for name in ('runtime_feature_demand.h', 'control_link.h',
        'hotkey_routes.h', 'type_hotkey.h', 'hook_policy.h', 'destruct_owner_input.h',
        'placement_event_input.h', 'protocol.h', 'additional_settings.h', 'automatic_features.h')]
    paths += ['source/shared/source/key_gesture.h']
    sources = {path: digest(root / path) for path in paths}
    results = {}
    for name, callsite in [('current', found.group()), ('baseline', (root / BASELINE).read_text())]:
        directory = out / name
        directory.mkdir(parents=True, exist_ok=True)
        wrapper = 'static void oneTick(){\n' + registration + '\n' + callsite + '\n}\n'
        cpp = directory / 'production.cpp'
        cpp.write_text(preamble + functions + wrapper + TESTS)
        command = ['g++', '-std=c++17', '-O2', '-UNDEBUG', '-DRA2_UNIFIED=1', '-MMD', '-MF', str(directory / 'production.d'),
                   '-I', str(root / 'source/assist/source'), str(cpp), '-o', str(directory / 'production')]
        built = subprocess.run(command, capture_output=True, text=True)
        (directory / 'compile.log').write_text(built.stdout + built.stderr)
        if built.returncode:
            raise RuntimeError(name + ' failed to compile: ' + built.stderr)
        executed = subprocess.run([str(directory / 'production')], capture_output=True, text=True)
        raw = executed.stdout + executed.stderr
        (directory / 'run.log').write_text(raw)
        objects = [json.loads(line) for line in executed.stdout.splitlines() if line.startswith('{')]
        if not objects or objects[-1].get('suite') != 'hotkey_cold_start':
            raise RuntimeError('Missing actual test report: ' + raw)
        results[name] = {'summary': objects[-1], 'cases': objects[:-1], 'compile_command': command,
                         'compile_exit_code': built.returncode, 'exit_code': executed.returncode,
                         'stdout': executed.stdout, 'stderr': executed.stderr,
                         'run_log_sha256': digest(directory / 'run.log'),
                         'callsite_sha256': hashlib.sha256(callsite.encode()).hexdigest()}
    current, old = results['current'], results['baseline']
    failed = {case['case'] for case in old['cases'] if not case['passed']}
    if current['exit_code'] != 0 or current['summary']['passed'] is not True:
        raise RuntimeError('Current cold-start flow failed: ' + current['stdout'])
    if old['exit_code'] != 1 or failed != {'manual_only', 'manual_restart', 'delayed_connection'}:
        raise RuntimeError('Negative control did not reproduce the same cold cycle: ' + old['stdout'])
    for path, sha in sources.items():
        if digest(root / path) != sha:
            raise RuntimeError('Source changed during cold-start run: ' + path)
    result = {'suite': 'hotkey_cold_start', 'passed': True, 'scope': SCOPE,
              'scenarios': current['summary']['scenarios'], 'checks': current['summary']['checks'],
              'baseline_callsite_sha256': BASELINE_SHA, 'negative_control_reproduced': True,
              'sources': sources, **results, 'windows_executed': False, 'game_tested': False}
    (out / 'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    print(json.dumps(run(args.source_root.resolve(), args.out.resolve()), ensure_ascii=False))
