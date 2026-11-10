"""Run the shipped reservation admission functions with explicit native/probe seams.

The guard/probe implementation is unchanged. This regression checks that
reservations actually reach it after an ordinary native denial, while retaining
full-mode build limits, inactive-mode denial and native ordinary production.
It does not emulate a platform or establish live multiplayer compatibility.
"""
from pathlib import Path
import json
import hashlib
import os
import subprocess
import tempfile
from tactical_frame_sampling import function

src = Path(__file__).resolve().parents[1] / 'source'
sidebar = (src / 'sidebar.h').read_text()
parameters = (src / 'tech_ui_breakpoint_bridge.h').read_text()
signature = 'static bool techEnhancementReservationCanBuild('
functions = function(parameters, signature) if signature in parameters else ''
functions += '\n' + function(sidebar, 'static bool reservationCanBuild(')
prefix = r'''
#include <cassert>
#include <cstdint>
#include <iostream>
static bool dependency=true,fullText=false,fullParameters=false,exact=false;
static int nativeResult=0,probeResult=0,nativeCalls=0,probeCalls=0;
static uint32_t lastType=0,lastHouse=0;static bool lastFull=false;
static bool nativeDependencyReady(uint32_t p){assert(p==0x4F7870);return dependency;}
static bool techGate(uint32_t,uint32_t){return fullText;}
static bool techParameterReservationAllowed(uint32_t,uint32_t){return fullParameters;}
namespace techparameters {static bool enhancedRequested(){return exact;}}
static int GuardedTechEnhancementProbe(uint32_t house,uint32_t type){
 ++probeCalls;assert(house==lastHouse&&type==lastType);return probeResult;
}
template<class T>static T call(uint32_t p,uint32_t house,void*type,bool full,bool include){
 assert(p==0x4F7870&&include);++nativeCalls;lastHouse=house;
 lastType=uint32_t(uintptr_t(type));lastFull=full;return T(nativeResult);
}
'''
suffix = r'''
int main(){
 unsigned cases=0;
 for(bool text:{false,true})for(bool parameters:{false,true})for(bool enhanced:{false,true})
 for(int original:{0,-1,1})for(int probe:{1,0,-1}){
  fullText=text;fullParameters=parameters;exact=enhanced;nativeResult=original;probeResult=probe;
  nativeCalls=probeCalls=0;
  const bool full=text||parameters;
  bool result=reservationCanBuild(0x20010000,0x20020000);
  assert(result==(original>0||(!full&&enhanced&&probe>0)));
  assert(nativeCalls==1&&lastFull==full);
  assert(probeCalls==int(original<=0&&!full&&enhanced));++cases;
 }
 nativeCalls=probeCalls=0;assert(!reservationCanBuild(0,0x20020000));
 dependency=false;assert(!reservationCanBuild(0x20010000,0x20020000));
 assert(!nativeCalls&&!probeCalls);cases+=2;
 std::cout<<"PASS: "<<cases<<" production admission cases; native/full limits preserved; exact probe reached only after ordinary denial\n";
}
'''
with tempfile.TemporaryDirectory() as temporary:
    cpp = Path(temporary) / 'admission.cpp'
    exe = Path(temporary) / 'admission'
    cpp.write_text(prefix + functions + suffix)
    subprocess.run(['g++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
print(json.dumps({'source_sha256': {name: hashlib.sha256((src/name).read_bytes()).hexdigest()
                                  for name in ['sidebar.h', 'tech_ui_breakpoint_bridge.h']},
                  'native_calls_modeled': True, 'live_game_tested': False}))
