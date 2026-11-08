
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
static std::unique_ptr<Shared> storage=std::make_unique<Shared>();
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
 std::memset(shared,0,sizeof *shared);snap={};incoming={};extra={};extra.constructionQueue=true;
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
static void publish(bool busy,bool changedType){
 // Exact forced scheduling point: queue peek/consume already completed against match 7.
 check(commandQueue.size==0&&commandMatch()==7);++fakeNow;
 shared->seq=3;shared->data=incoming;shared->data.match=8;shared->data.frame=10;
 shared->data.commandStatus=busy?0:1;if(changedType)std::strcpy(shared->data.types[0].id,"DIFFERENT");
 MemoryBarrier();shared->seq=4;
}
int main(){
 bool comma=false;std::cout<<"{\"cases\":[";
 for(int command:{ProduceDeferred,ProduceReserved})for(int mode=0;mode<3;++mode){
  setup(command);const auto hash=commandQueue.slots[0].typeHash;beforeSnapshot=[=]{publish(mode==1,mode==2);};
  serviceCommandQueue(fakeNow);check(commandMatch()==8&&snapshotSeq==4&&lastData==fakeNow);
  const int tab=command==ProduceReserved?0:3;
  const uint32_t request=shared->request,requestMatch=shared->requestMatch;const int queued=commandQueue.size;const uint32_t queuedMatch=queued?commandQueue.slots[0].match:0;
  if(mode==0){check(shared->request==1&&shared->command==command&&shared->requestMatch==8&&shared->requestToken==51&&shared->requestTypeHash==hash);check(commandQueue.size==0&&constructionReceipts.empty());}
  if(mode==1){check(shared->request==0&&commandQueue.size==1&&commandQueue.slots[0].match==8&&commandQueue.slots[0].token==51&&commandQueue.slots[0].typeHash==hash);check(constructionReceipts.empty());}
  if(mode==2){check(shared->request==0&&commandQueue.size==0&&constructionReceipts.size()==1);check(std::get<0>(constructionReceipts[0])==-13&&std::get<3>(constructionReceipts[0])==51);}
  // Strongest subsequent Book cleanup uses the actual shipped cancel function.
  buildreserve::cancel(tab);check(buildreserve::books[tab].items.empty()&&commandQueue.size==0);check(shared->request==request&&shared->requestMatch==requestMatch);
  if(comma)std::cout<<',';comma=true;
  std::cout<<"{\"command\":\""<<(command==ProduceDeferred?"ProduceDeferred":"ProduceReserved")<<"\",\"mode\":\""<<(mode==0?"new_match_ready":mode==1?"new_match_busy":"changed_catalogue")<<"\",\"original_match\":7,\"observed_match\":"<<commandMatch()<<",\"published_request\":"<<request<<",\"request_match\":"<<requestMatch<<",\"queued_before_book_cancel\":"<<queued<<",\"queued_match\":"<<queuedMatch<<",\"request_after_book_cancel\":"<<shared->request<<",\"construction_rejection_count\":"<<constructionReceipts.size()<<'}';
 }
 std::cout<<"],\"checks\":"<<checks<<",\"passed\":true,\"native_event_executed\":false}\n";
}
