#!/usr/bin/env python3
"""Count the real frameobserver::maintain under controlled OS/patch boundaries.

This extracts the production state, releaseChannel and maintain bodies unchanged.
It omits the x86 committedAdapter (never executed here) and substitutes Win32 IPC,
clock, safe reads and owned-patch service boundaries. It is not a Windows/game,
inline-patch ABI or multiplayer test. No wall-time speedup is inferred.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--header', type=Path)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--expect-optimized', action='store_true')
p.add_argument('--cxx', default='g++')
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
header = a.header or root/'source/game_runtime/source/frame_signal_runtime.h'
source = header.read_text()
start = source.index('namespace frameobserver {')
callback = source.index('extern "C" void RuntimeFrameReady();', start)
release = source.index('static void releaseChannel()', callback)
production = source[start:callback]+'static void committedAdapter(){}\n'+source[release:]
out = a.out.resolve(); out.mkdir(parents=True, exist_ok=True)
(out/'windows.h').write_text(r'''
#pragma once
#include <cstdint>
#include <cstddef>
using DWORD=uint32_t;using LONG=int32_t;using SIZE_T=size_t;using HANDLE=void*;using BOOL=int;
struct LARGE_INTEGER{int64_t QuadPart;};
constexpr int FALSE=0,FILE_MAP_ALL_ACCESS=1,EVENT_MODIFY_STATE=2;
inline LONG InterlockedCompareExchange(volatile LONG*p,LONG n,LONG o){LONG v=*p;if(v==o)*p=n;return v;}
inline LONG InterlockedExchange(volatile LONG*p,LONG n){LONG v=*p;*p=n;return v;}
inline void MemoryBarrier(){}
''')
preamble = r'''
#include <windows.h>
#include <cassert>
#include <cstring>
#include <cwchar>
#include <iostream>
#include "FRAME_PROTOCOL"
#include "PATCH_GATE"
struct Counters{unsigned reads=0,opens=0,maps=0,events=0,unmaps=0,closes=0,installs=0,restores=0,scopes=0;};
static Counters counts;
static DWORD tick=1000,error=0;
static framesignal::Channel offered{};
static bool available=false,eventAvailable=true,mapAvailable=true,active=false,foreignConflict=false;
static unsigned failRead=0,restoreMode=0,installMode=0;
static int session=0,frame=30;
static uint32_t house=0x10000;
static uint8_t running=1,oos=0;
static DWORD GetLastError(){return error;}
static void SetLastError(DWORD e){error=e;}
static DWORD GetTickCount(){return tick;}
static DWORD GetCurrentProcessId(){return 123;}
static DWORD GetCurrentThreadId(){return 456;}
static HANDLE GetCurrentProcess(){return (HANDLE)1;}
static bool ReadProcessMemory(HANDLE,const void* p,void*out,size_t n,SIZE_T*got){
 ++counts.reads;*got=0;error=299;if(failRead&&counts.reads==failRead)return false;
 const uintptr_t at=uintptr_t(p);const void*value=nullptr;size_t size=0;
 if(at==0xA8B238){value=&session;size=4;}else if(at==0xA83D4C){value=&house;size=4;}
 else if(at==0xA8ED84){value=&frame;size=4;}else if(at==0xA8E9A0){value=&running;size=1;}
 else if(at==0xA8B8C2){value=&oos;size=1;}assert(value&&size==n);memcpy(out,value,n);*got=n;return true;
}
static HANDLE OpenFileMappingW(int,BOOL,const wchar_t*name){
 ++counts.opens;error=2;wchar_t expected[96];framesignal::name(expected,123,offered.owner);
 return available&&!wcscmp(name,expected)?(HANDLE)2:nullptr;
}
static void*MapViewOfFile(HANDLE,int,int,int,size_t){++counts.maps;error=5;return mapAvailable?&offered:nullptr;}
static HANDLE OpenEventW(int,BOOL,const wchar_t*){++counts.events;error=6;return eventAvailable?(HANDLE)3:nullptr;}
static void UnmapViewOfFile(void*){++counts.unmaps;error=7;}
static void CloseHandle(HANDLE){++counts.closes;error=8;}
static void QueryPerformanceFrequency(LARGE_INTEGER*f){f->QuadPart=1000000;error=9;}
namespace debugentries{static uint32_t policyRevision=1;}
namespace gameentry{
 enum class Client{Assist};
 template<class F>void scoped(Client,bool,F f){++counts.scopes;f();}
 struct Inspection{bool active;unsigned conflict;};
 static Inspection inspect(uint32_t){return {::active,::foreignConflict?1u:0u};}
 static bool patch(Client,uint32_t,const uint8_t*,const uint8_t*to,size_t n){
  assert(n==6);error=1234;
  if(to[0]==0xE9){++counts.installs;if(installMode==1)return false;::active=true;return installMode!=2;}
  ++counts.restores;if(restoreMode==1)return false;::active=false;return restoreMode!=2;
 }
}
'''.replace('FRAME_PROTOCOL', str(root/'source/shared/source/frame_signal_protocol.h')).replace('PATCH_GATE',str(root/'source/shared/source/multiplayer_patch_gate.h'))
driver=r'''
static void call(DWORD requested){error=0xA55A;frameobserver::maintain(requested);assert(error==0xA55A);}
static void reset(DWORD now=1000){
 frameobserver::installed=false;frameobserver::conflict=false;frameobserver::releaseChannel();
 active=false;foreignConflict=false;available=false;eventAvailable=mapAvailable=true;
 restoreMode=installMode=failRead=0;session=0;house=0x10000;frame=30;running=1;oos=0;tick=now;
 offered={};offered.magic=framesignal::Magic;offered.version=framesignal::Version;offered.pid=123;offered.owner=7;
 offered.enabled=framesignal::Compute|framesignal::ExplicitDiagnostic;offered.heartbeat=tick;
 call(0);counts={};
}
static void report(const char*name,bool comma=true){
 std::cout<<'"'<<name<<"\":{\"reads\":"<<counts.reads<<",\"mapping_probes\":"<<counts.opens
 <<",\"maps\":"<<counts.maps<<",\"events\":"<<counts.events<<",\"unmaps\":"<<counts.unmaps
 <<",\"closes\":"<<counts.closes<<",\"installs\":"<<counts.installs<<",\"restores\":"<<counts.restores
 <<",\"scopes\":"<<counts.scopes<<'}'<<(comma?",":"");
}
int main(){
 std::cout<<'{';
 reset();for(unsigned i=0;i<100000;++i){tick=1000+i/100;call(7);}report("absent_100000_messages_1s");
 reset();for(unsigned i=0;i<10000;++i)call(0);report("no_controller_10000");
 reset();available=true;offered.enabled=framesignal::Compute;
 for(unsigned i=0;i<1000;++i){tick=1000+i;offered.heartbeat=tick;call(7);}
 assert(frameobserver::channel==&offered&&!active&&offered.state==framesignal::MessageFallback);report("ordinary_consumer_1000");
 reset(2000);call(7);tick=2050;available=true;offered.heartbeat=tick;call(7);
 const bool openedBeforeDeadline=frameobserver::channel!=nullptr;
 tick=2100;offered.heartbeat=tick;call(7);assert(frameobserver::installed&&active&&offered.state==framesignal::Native);
 assert(offered.thread==456&&offered.frequency==1000000);report("late_diagnostic");
 counts={};for(unsigned i=0;i<1000;++i){tick=2101+i;offered.heartbeat=tick;call(7);}
 assert(active&&frameobserver::installed);report("installed_1000");
 counts={};tick+=600;call(7);assert(active&&frameobserver::installed);report("stale_installed_retained");
 counts={};offered.enabled=framesignal::Compute;call(7);assert(!active&&!frameobserver::installed&&offered.state==framesignal::MessageFallback);report("deselect_immediate");
 counts={};offered.enabled|=framesignal::ExplicitDiagnostic;offered.heartbeat=tick;call(7);assert(active&&offered.state==framesignal::Native);report("reenable_immediate");
 counts={};call(0);assert(!active&&!frameobserver::channel&&!frameobserver::owner);report("controller_death_immediate");
 reset();available=true;call(7);restoreMode=1;call(8);
 assert(active&&frameobserver::channel==&offered&&frameobserver::owner==7&&offered.state==framesignal::RestoreFailed);
 restoreMode=2;call(8);assert(!active&&!frameobserver::channel&&!frameobserver::owner);report("owner_change_restore_failure");
 reset();available=true;installMode=2;call(7);assert(active&&frameobserver::installed&&offered.state==framesignal::SignatureRejected);
 call(0);assert(!active&&!frameobserver::channel);report("failed_install_after_write_cleanup");
 reset();available=true;installMode=1;call(7);assert(!active&&offered.state==framesignal::SignatureRejected);
 installMode=0;call(7);assert(!active);++debugentries::policyRevision;call(7);assert(active);report("policy_retry");
 for(unsigned bad=0;bad<8;++bad){reset();available=true;
  if(bad==0)offered.magic=0;else if(bad==1)offered.version=0;else if(bad==2)offered.pid=999;
  else if(bad==3)offered.owner=9;else if(bad==4)offered.enabled=0;else if(bad==5)offered.heartbeat=tick-501;
  else if(bad==6)mapAvailable=false;else eventAvailable=false;
  call(7);assert(!active&&!frameobserver::channel&&!frameobserver::mapping&&!frameobserver::event);
 }
 for(int mode:{0,3,4,5}){reset();available=true;session=mode;call(7);assert(active);call(0);assert(!active);}
 for(unsigned bad=0;bad<7;++bad){reset();available=true;
  if(bad==0)session=1;else if(bad==1)house=0;else if(bad==2)frame=-1;else if(bad==3)running=0;
  else if(bad==4)oos=1;else if(bad==5)failRead=1;else failRead=6;
  call(7);assert(!active&&(offered.state==framesignal::Waiting||offered.state==framesignal::MessageFallback));
 }
 reset(0xFFFFFFD0u);call(7);tick=0x20;call(7);tick=0x34;call(7);report("clock_wrap");
 reset();call(7);tick+=1;available=true;offered.owner=8;offered.heartbeat=tick;call(8);assert(active&&frameobserver::owner==8);report("new_owner_immediate",false);
 std::cout<<",\"opened_before_discovery_deadline\":"<<(openedBeforeDeadline?"true":"false")<<",\"semantic_assertions\":\"passed\"}";
}
'''
probe=out/'maintain-production.cpp';probe.write_text(preamble+production+driver)
exe=out/'maintain-production'
cmd=[a.cxx,'-std=c++17','-O2','-UNDEBUG','-I',str(out),str(probe),'-o',str(exe)]
subprocess.run(cmd,check=True)
run=subprocess.run([str(exe)],check=True,capture_output=True,text=True)
result=json.loads(run.stdout)
if a.expect_optimized:
 assert result['absent_100000_messages_1s']['reads']==0
 assert result['absent_100000_messages_1s']['mapping_probes']==10
 assert result['no_controller_10000']['reads']==0
 assert result['ordinary_consumer_1000']['reads']==0
 assert result['installed_1000']['reads']==0
 assert result['stale_installed_retained']['reads']==0
 assert result['deselect_immediate']['reads']==0
 assert result['controller_death_immediate']['reads']==0
 assert result['clock_wrap']['mapping_probes']==2
 assert not result['opened_before_discovery_deadline']
result.update(production_header=str(header.resolve()),production_sha256=hashlib.sha256(header.read_bytes()).hexdigest(),
 source_extraction='verbatim namespace state, releaseChannel and maintain; committed callback omitted',
 command=cmd,scope='Linux native controlled boundary execution of production maintain; Windows IPC and patch writer stubbed',
 game_tested=False,windows_tested=False,multiplayer_tested=False,performance_claim='call counts only; no FPS or wall-time speedup claim')
(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
