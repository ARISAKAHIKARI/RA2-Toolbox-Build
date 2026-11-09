"""Execute real display admission/selection functions with independent clocks.

The Win32 clock, process reader and final selection sink are controlled. The
production mergeExternal, watch synchronizeWorld, watch lifetime and aircraft
selection code execute unchanged. The frozen pre.5 functions form a compiled
negative control; no Windows UI or real game execution is claimed.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess

BASELINE = 'source/assist/tests/fixtures/entry_pre1475/display_entry_clocks.json'
BASELINE_SHA = '4889fb338cd23508036b5dc0d4be14161798a3de2e1ac194c9ae6ff76eba5ff3'
BOUNDARY = 'source/research/evidence/pre147-hotkey-origin/bootstrap/cold_bootstrap.py'
SCOPE = 'actual Host merge/watch-lifetime/aircraft-selection functions; controlled independent Win32 clocks, process memory and selection sink; no Windows/game execution'

EXTRA = r'''
static unsigned selections=0;static uint32_t selectedID=0;
static void sendSelection(int type,bool global,const uint8_t*,uint32_t id){if(type!=-4||global)throw std::runtime_error("unexpected selection route");++selections;selectedID=id;}
static uint32_t lastIdleAircraft=0,lastAircraftMatch=0,lastAircraftPID=0;
namespace watchentry {
static uint32_t process=0,epoch=0,controls=0;
static watchworld::Lifetime lifetime;static uint32_t lifetimeSample=0;static bool lifetimeReady=false;
static unsigned resets=0;
struct Worker{void invalidate(){}};static Worker worker;static std::shared_ptr<int>applied;
static void reset(){++resets;lifetime.reset();lifetimeSample=0;lifetimeReady=false;}
}
'''
TESTS = r'''
static unsigned checks=0,failures=0,scenarios=0,caseFailures=0;
static void check(bool ok,const char*label){++checks;if(!ok){++failures;++caseFailures;std::cout<<"CHECK_FAILED "<<label<<"\n";}}
static void reset(uint32_t stamp,uint32_t control,uint32_t render){
 now=control;renderClock=render;lastData=control;shared=storage.get();shared->stop=0;
 incoming={};incoming.status=incoming.bridgeStatus=incoming.commandStatus=1;incoming.match=1;
 snap=incoming;snap.renderStatus=snap.readStatus=1;snap.width=1024;snap.height=768;
 externalPublished=snap;externalPublished.sampledTick=stamp;externalPublished.frame=10;
 externalData=&externalPublished;externalSourceConfirmed=false;externalWorldSource.reset();
 externalSampler.idleAircraftTick=stamp;externalSampler.idleAircraftCount=2;
 externalSampler.idleAircraftIDs[0]=101;externalSampler.idleAircraftIDs[1]=202;
 enabled=focused=true;worldReadable=true;lastIdleAircraft=lastAircraftMatch=lastAircraftPID=0;
 selections=0;selectedID=0;
 watchentry::lifetime.reset();watchentry::lifetimeSample=watchentry::process=watchentry::epoch=0;watchentry::lifetimeReady=false;
}
static void finish(const char*name){++scenarios;std::cout<<"{\"case\":\""<<name<<"\",\"passed\":"<<(caseFailures?"false":"true")<<",\"failures\":"<<caseFailures<<"}\n";caseFailures=0;}
static void clocks(const char*name,uint32_t stamp,uint32_t control,uint32_t render,bool display,bool aircraft){
 reset(stamp,control,render);mergeExternal();
 check((snap.readStatus==1)==display,"display read readiness follows producer clock");
 check((snap.renderStatus==1)==display,"render admission follows producer clock");
 check(watchentry::synchronizeWorld()==display,"watch admission follows producer clock");
 selectNextIdleAircraft();check((selections==1)==aircraft,"aircraft intent follows producer clock and original TTL");
 if(aircraft)check(selectedID==101,"unchanged first aircraft route");finish(name);
}
int main(){
 clocks("same_clock_recent",1000,1000,1000,true,true);
 clocks("render_clock_behind",1008,1008,1000,true,true);
 clocks("render_clock_far_ahead",1000,1000,4000,true,true);
 clocks("stale_real_clock_render_frozen",1000,3000,1000,false,false);
 clocks("aircraft_at_500ms",1000,1500,900,true,true);
 clocks("aircraft_after_500ms",1000,1501,1000,true,false);
 clocks("display_before_2000ms",1000,2999,1000,true,false);
 clocks("display_at_2000ms",1000,3000,1000,false,false);
 clocks("wrap_recent",0xfffffff0u,16,8,true,true);
 clocks("zero_stamp_rejected",0,1000,1000,false,false);
 reset(1000,5000,4000);externalSourceConfirmed=true;mergeExternal();
 check(snap.readStatus==1&&snap.renderStatus==1,"confirmed immutable display keeps paused lease");
 selectNextIdleAircraft();check(selections==0,"display confirmation never renews command selection TTL");finish("confirmed_display_not_input");
 reset(1008,1008,1000);snap.chat=true;selectNextIdleAircraft();check(selections==0,"chat blocks selection");
 snap.chat=false;focused=false;selectNextIdleAircraft();check(selections==0,"foreign focus blocks selection");
 focused=true;enabled=false;selectNextIdleAircraft();check(selections==0,"disabled host blocks selection");finish("input_context_guards");
 reset(1008,1008,1000);worldReadable=false;check(!watchentry::synchronizeWorld(),"missing world blocks watch");
 worldReadable=true;++externalPublished.sampledTick;now=externalPublished.sampledTick;
 check(watchentry::synchronizeWorld(),"new complete observation recovers watch");finish("watch_read_failure_recovery");
 reset(1008,1008,1000);mergeExternal();selectNextIdleAircraft();selectNextIdleAircraft();check(selectedID==202,"aircraft cycle advances");
 ++incoming.match;externalPublished.match=incoming.match;selectNextIdleAircraft();check(selectedID==101,"new command epoch resets aircraft cycle");finish("selection_epoch_recovery");
 std::cout<<"{\"suite\":\"display_entry_clock\",\"passed\":"<<(failures?"false":"true")<<",\"scenarios\":"<<scenarios<<",\"checks\":"<<checks<<",\"failures\":"<<failures<<"}\n";
 return failures?1:0;
}
'''

def load_module(path, name):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def run(root:Path,out:Path,allow_current_failure:bool=False):
    assert sha(root/BASELINE)==BASELINE_SHA
    extract=load_module(root/'source/assist/tests/hotkey_command_context.py','clock_extract')
    boundary=load_module(root/BOUNDARY,'clock_boundary')
    main=(root/'source/assist/source/main.cpp').read_text()
    old=json.loads((root/BASELINE).read_text())['functions']
    sources={name:(root/row['path']).read_text() for name,row in old.items()}
    support='\n'.join(extract.function(main,n) for n in ['commandLink','commandMatch','commandChannelReady','commandFresh','discardExternalWorld'])
    preamble=boundary.PREAMBLE
    preamble=preamble.replace('#include "hook_policy.h"','#include "hook_policy.h"\n#include "watch_world_lifetime.h"')
    preamble=preamble.replace('static DWORD sampleRenderClock(void*){return now;}','static DWORD renderClock=10000;static DWORD sampleRenderClock(void*){return renderClock;}')
    preamble=preamble.replace('Snapshot value{};unsigned flashCount=0;','Snapshot value{};unsigned flashCount=0;uint32_t idleAircraftTick=0,idleAircraftIDs[8192]{};int idleAircraftCount=0;')
    marker='struct ReaderBoundary{'
    memory=r'''static bool worldReadable=true;
static bool worldRead(uint32_t p,void*out,size_t n){
 if(!worldReadable)return false;
 uint32_t v=0;
 switch(p){case 0xA8B230:v=0x100000;break;case 0xA8B238:v=0;break;case 0x87F924:v=0x200000;break;case 0x87F928:v=64;break;case 0xA8ED84:v=10;break;case 0xA8E9A0:v=1;break;default:return false;}
 std::memcpy(out,&v,n);return true;
}
'''
    preamble=preamble.replace(marker,memory+marker)
    preamble=preamble.replace('bool readFresh(uint32_t p,void*out,size_t n){return readLive(p,out,n);}','bool readFresh(uint32_t p,void*out,size_t n){return worldRead(p,out,n);}')
    paths=[BASELINE,BOUNDARY,'source/assist/tests/display_entry_clock.py','source/assist/tests/hotkey_command_context.py',
           'source/assist/source/main.cpp','source/assist/source/watch_host.h','source/assist/source/idle_aircraft_selection.h',
           'source/assist/source/aircraft_indicator.h','source/assist/source/clock_ms.h',
           'source/assist/source/watch_world_lifetime.h','source/assist/source/world_lifecycle.h','source/assist/source/protocol.h','source/assist/source/read_only_lifecycle.h']
    binding={p:sha(root/p) for p in paths}
    results={}
    for variant in ['current','pre5']:
        functions={name:extract.function(sources[name],name) if variant=='current' else row['function'] for name,row in old.items()}
        cpp=preamble+EXTRA+support+'\n'+functions['mergeExternal']+'\n'+functions['selectNextIdleAircraft']+'\nnamespace watchentry {\n'+functions['synchronizeWorld']+'\n}\n'+TESTS
        directory=out/variant;directory.mkdir(parents=True,exist_ok=True)
        path=directory/'production.cpp';path.write_text(cpp)
        command=['g++','-std=c++17','-O2','-UNDEBUG','-MMD','-MF',str(directory/'production.d'),'-I',str(root/'source/assist/source'),str(path),'-o',str(directory/'production')]
        compiled=subprocess.run(command,text=True,capture_output=True)
        (directory/'compile.log').write_text(compiled.stdout+compiled.stderr)
        if compiled.returncode:raise RuntimeError(variant+' compile failed: '+compiled.stderr)
        executed=subprocess.run([str(directory/'production')],text=True,capture_output=True)
        raw=executed.stdout+executed.stderr;(directory/'run.log').write_text(raw)
        rows=[json.loads(line) for line in executed.stdout.splitlines() if line.startswith('{')]
        if not rows or rows[-1].get('suite')!='display_entry_clock':raise RuntimeError('Missing execution summary')
        results[variant]={'compile_command':command,'compile_exit_code':0,'exit_code':executed.returncode,'summary':rows[-1],'cases':rows[:-1],'run_log_sha256':sha(directory/'run.log')}
    assert all(sha(root/p)==value for p,value in binding.items()),'source changed during check'
    result={'suite':'display_entry_clock','passed':results['current']['exit_code']==0 and results['pre5']['exit_code']==1,'scope':SCOPE,
            'source_sha256':binding,'baseline_fixture_sha256':BASELINE_SHA,'results':results,'windows_executed':False,'game_tested':False}
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ['source_sha256','results']}|{'current':results['current']['summary'],'pre5':results['pre5']['summary']}))
    if not result['passed'] and not allow_current_failure:raise RuntimeError('Clock regression did not meet current/old expectations')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--root',type=Path,required=True);parser.add_argument('--out',type=Path,required=True);parser.add_argument('--allow-current-failure',action='store_true')
    args=parser.parse_args();run(args.root.resolve(),args.out.resolve(),args.allow_current_failure)
