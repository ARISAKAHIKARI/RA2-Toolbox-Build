"""Run the actual reservation Host send/queue/snapshot/cancel code at a world boundary.
The only controlled boundary is publication between queue consumption and the
real readSnapshot. No native game event, Windows runtime or multiplayer claim.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess,tempfile
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path)
parser.add_argument('--main-source',type=Path,help='Optional immutable before-fix main.cpp; shared helpers still come from this tree.')
parser.add_argument('--expect-regression',action='store_true')
args=parser.parse_args()
repo=Path(__file__).resolve().parents[3]
src=repo/'source/assist/source'
temporary=tempfile.TemporaryDirectory() if args.output is None else None
out=args.output or Path(temporary.name)
out.mkdir(parents=True,exist_ok=True)
files=['main.cpp','protocol.h','command_delivery.h','control_link.h','additional_settings.h','construction_host.h','construction_policy.h']
paths={n:(args.main_source if n=='main.cpp' and args.main_source else src/n) for n in files}
texts={n:p.read_text() for n,p in paths.items()}
hashes={n:hashlib.sha256(p.read_bytes()).hexdigest() for n,p in paths.items()}
def function(text,signature):
 start=text.index(signature);brace=text.index('{',start);end=brace+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
main=texts['main.cpp']
parts={
 'identity.inc':'\n'.join(function(main,s) for s in ['static controllink::View commandLink(){','static uint32_t commandMatch(){','static bool commandChannelReady(){','static bool commandFresh(DWORD now){']),
 'snapshot.inc':function(main,'static void readSnapshot(){'),
 'send.inc':'\n'.join(function(main,s) for s in ['static bool userCommand(','static bool queueCommand(','static void send(int command']),
 'service.inc':function(main,'static void serviceCommandQueue(DWORD now){'),
 'cancel.inc':function(texts['construction_host.h'],'static void cancel(int tab){')}
for n,s in parts.items():(out/n).write_text(s+'\n')
program=r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include "protocol.h"
#include "additional_settings.h"
#include "command_delivery.h"
#include "control_link.h"
#include "construction_policy.h"
using namespace assist;
using DWORD=uint32_t;using LONG=int32_t;using HWND=void*;
static std::unique_ptr<Shared> storage=std::make_unique<Shared>(),replacement=std::make_unique<Shared>();
static Shared*shared=storage.get();static Snapshot snap{},incoming{};static AdditionalSettings extra;
static delivery::Queue commandQueue;
static bool enabled=true,focus=true,window=true,hasSelection=false,hookSmartUnload=true,bridgeBlocked=false,techMode=false;
static uint32_t pid=123,ack=0,inFlightMatch=0,snapshotSeq=2,autoRevealMatch=0,autoRevealDone=0;
static DWORD fakeNow=1000,lastData=1000,lastFind=0,sentAt=0;
static int inFlightRetries=0,bridgeAttempts=0,techImplementation=0,techProfile=0,wakes=0,checks=0;
static uint64_t techAcknowledgedHash=0;static HWND game=reinterpret_cast<HWND>(uintptr_t(2));
static std::function<void()> beforeSnapshot;static std::vector<std::tuple<int,int,uint32_t,uint32_t>> constructionReceipts;
static DWORD GetTickCount(){return fakeNow;}static bool IsWindow(HWND h){return h&&window;}
static void MemoryBarrier(){std::atomic_thread_fence(std::memory_order_seq_cst);}
static LONG InterlockedIncrement(volatile LONG*p){return ++*p;}
static bool commandFocus(){return focus;}static bool foregroundGame(){return focus;}
static DWORD discoverGamePid(){return pid;}static void ensureBridge(){assert(false&&"fixture retains same live mapping");}static void wakeGame(){++wakes;}
static bool techAllowed(const Type&){return false;}
static void constructionResult(int r,int n,uint32_t id,uint32_t token){constructionReceipts.emplace_back(r,n,id,token);}
static void logCommandWait(){}static void diag(const std::wstring&){}static void critical(const std::wstring&){}static void notify(const std::wstring&){}
static std::wstring commandWaitReason(){return L"fixture";}
static void logCommandReceipts(const Snapshot&){}static void resetStartupProbeDiagnostics(){}static void logStartupProbeDifferences(bool){}static void logFeatureActivation(DWORD){}
// No external display Snapshot is attached: real mergeExternal immediately returns in this state.
static void mergeExternal(){}
namespace autolog{static void write(const std::wstring&){}}
static struct{uint64_t generation(){return 1;}bool readFresh(uint32_t,void*out,size_t n){std::memset(out,0,n);return true;}}externalReader;
// Unrelated RevealMap path is never used in these reservation scenarios.
namespace assist{namespace revealpermission{template<class F>void beforeRequest(volatile uint32_t&,bool,F f){f();}}}
#include "identity.inc"
#define readSnapshot readSnapshotCore
#include "snapshot.inc"
#undef readSnapshot
static void readSnapshot(){if(beforeSnapshot){auto callback=std::move(beforeSnapshot);beforeSnapshot={};callback();}readSnapshotCore();}
#include "send.inc"
#include "service.inc"
namespace buildreserve{static construction::Book books[4];
#include "cancel.inc"
}
static void check(bool okay){assert(okay);++checks;}
static void setup(int cmd){
 shared=storage.get();std::memset(replacement.get(),0,sizeof *replacement);std::memset(shared,0,sizeof *shared);snap={};incoming={};extra={};extra.constructionQueue=true;
 enabled=focus=window=true;hasSelection=false;bridgeBlocked=false;techMode=false;pid=123;ack=0;fakeNow=lastData=1000;snapshotSeq=2;wakes=0;beforeSnapshot={};
 inFlightMatch=0;inFlightRetries=0;autoRevealMatch=autoRevealDone=0;techAcknowledgedHash=0;
 snap.status=snap.renderStatus=incoming.status=incoming.commandStatus=1;snap.commandStatus=1;
 snap.match=incoming.match=7;snap.frame=incoming.frame=900;incoming.typeCount=snap.typeCount=1;
 auto&t=snap.types[0];std::strcpy(t.id,cmd==ProduceReserved?"GAPOWR":"MTNK");t.kind=cmd==ProduceReserved?7:40;t.index=0;
 incoming.types[0]=t;shared->data=incoming;shared->seq=2;shared->flags=ReserveConstruction;
 commandQueue.clear();constructionReceipts.clear();for(auto&b:buildreserve::books)b={};
 const int tab=cmd==ProduceReserved?0:3;auto&b=buildreserve::books[tab];b.items={0};b.match=7;b.house=0x500000;b.id=cmd==ProduceReserved?100:0;b.source=cmd==ProduceReserved?0x600000:0;b.stage=1;b.token=51;b.sent=fakeNow;
 check(queueCommand(cmd,0,int(b.id),b.match,0,0,b.token));check(commandQueue.size==1&&commandQueue.slots[0].match==7&&commandMatch()==7);
}

static void publish(int mode){
 // Queue peek/consume already completed against match 7; run the real
 // readSnapshot body immediately after this controlled publication.
 check(commandQueue.size==0&&commandMatch()==7);++fakeNow;
 if(mode==7){*replacement=*shared;shared=replacement.get();}
 if(mode==6)++pid;
 if(mode==8)fakeNow+=delivery::MaxAgeMs;
 shared->seq=3;shared->data=incoming;shared->data.match=mode<3?8:7;
 shared->data.frame=mode<3?10:901;shared->data.commandStatus=mode==1||mode==4?0:1;
 if(mode==2||mode==5)std::strcpy(shared->data.types[0].id,"DIFFERENT");
 MemoryBarrier();shared->seq=4;
}
int main(){
 bool comma=false;int rejected=0,unsafe=0,controls=0;std::cout<<"{\"cases\":[";
 const char*names[]={"new_match_ready","new_match_busy","new_match_changed_catalogue","same_match_ready","same_match_busy","same_match_changed_catalogue","same_match_pid_changed","same_match_connection_changed","same_match_expired_during_snapshot"};
 for(int command:{ProduceDeferred,ProduceReserved})for(int mode=0;mode<9;++mode){
  setup(command);const auto original=commandQueue.slots[0];beforeSnapshot=[=]{publish(mode);};
  serviceCommandQueue(fakeNow);check(commandMatch()==(mode<3?8u:7u)&&snapshotSeq==4&&lastData==fakeNow);
  const int tab=command==ProduceReserved?0:3;
  const uint32_t request=shared->request,requestMatch=shared->requestMatch;const int queued=commandQueue.size;
  const uint32_t queuedMatch=queued?commandQueue.slots[0].match:0,created=queued?commandQueue.slots[0].created:0;
  const bool reject=mode<3||mode>=5;
  const bool legacyLeak=EXPECT_REGRESSION&&reject&&mode!=2&&mode!=5;
  if(reject&&!legacyLeak){
   check(shared->request==0&&commandQueue.size==0&&constructionReceipts.size()==1);
   check(std::get<0>(constructionReceipts[0])==-13&&std::get<3>(constructionReceipts[0])==original.token);++rejected;
  }else if(mode==1||mode==4){
   check(shared->request==0&&commandQueue.size==1&&commandQueue.slots[0].match==(mode==1?8u:7u)&&commandQueue.slots[0].token==original.token&&commandQueue.slots[0].typeHash==original.typeHash);
   check(commandQueue.slots[0].created==(EXPECT_REGRESSION?fakeNow:original.created));check(constructionReceipts.empty());if(legacyLeak)++unsafe;else ++controls;
  }else{
   check(shared->request==1&&shared->command==command&&shared->requestMatch==(mode<3?8u:7u)&&shared->requestToken==original.token&&shared->requestTypeHash==original.typeHash);
   check(commandQueue.size==0&&constructionReceipts.empty());if(legacyLeak)++unsafe;else ++controls;
  }
  // The actual Book cleanup cannot recall a request that was already published.
  buildreserve::cancel(tab);check(buildreserve::books[tab].items.empty()&&commandQueue.size==0);
  check(shared->request==request&&shared->requestMatch==requestMatch);
  if(comma)std::cout<<',';comma=true;
  std::cout<<"{\"command\":\""<<(command==ProduceDeferred?"ProduceDeferred":"ProduceReserved")<<"\",\"mode\":\""<<names[mode]<<"\",\"original_match\":7,\"observed_match\":"<<commandMatch()<<",\"published_request\":"<<request<<",\"request_match\":"<<requestMatch<<",\"queued_before_book_cancel\":"<<queued<<",\"queued_match\":"<<queuedMatch<<",\"original_created\":"<<original.created<<",\"requeued_created\":"<<created<<",\"request_after_book_cancel\":"<<shared->request<<",\"construction_rejection_count\":"<<constructionReceipts.size()<<'}';
 }
 std::cout<<"],\"checks\":"<<checks<<",\"safe_rejections\":"<<rejected<<",\"unsafe_dispatch_or_requeue\":"<<unsafe<<",\"same_match_controls\":"<<controls<<",\"passed\":true,\"native_event_executed\":false}\n";
}
'''

cpp=out/'probe.cpp';cpp.write_text(program)
cmd=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-UNDEBUG','-DEXPECT_REGRESSION='+str(int(args.expect_regression)),'-I',str(src),str(cpp),'-o',str(out/'probe')]
built=subprocess.run(cmd,capture_output=True,text=True)
report={'scope':__doc__,'main_source_override':str(args.main_source) if args.main_source else None,
 'source_sha256':hashes,'extracted_blocks':{n:hashlib.sha256(s.encode()).hexdigest() for n,s in parts.items()},'compile_command':cmd,'compile_exit':built.returncode,'compile_stderr':built.stderr,'game_process_executed':False,'windows_runtime_executed':False,'expect_regression':args.expect_regression}
if built.returncode==0:
 import os
 ran=subprocess.run([str(out/'probe')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
 report.update(run_exit=ran.returncode,stdout=ran.stdout,stderr=ran.stderr)
 if ran.returncode==0:report['result']=json.loads(ran.stdout)
report['source_hash_stable']=all(hashlib.sha256(paths[n].read_bytes()).hexdigest()==h for n,h in hashes.items())
(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k in ['scope','compile_exit','compile_stderr','run_exit','result','stderr','source_hash_stable']},ensure_ascii=False,indent=2))
raise SystemExit(report.get('run_exit',built.returncode))
