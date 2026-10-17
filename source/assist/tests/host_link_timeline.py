"""Compile the shipped host identity/merge code and exercise delayed snapshots.
Optional --source PATH demonstrates the regression in an older main.cpp.
"""
from pathlib import Path
import subprocess, tempfile, sys
root=Path(__file__).resolve().parents[2]
source=Path(sys.argv[2]) if len(sys.argv)>2 and sys.argv[1]=='--source' else root/'assist/source/main.cpp'
main=source.read_text()
if 'static controllink::View commandLink()' in main:
    link='static controllink::View commandLink()'+main.split('static controllink::View commandLink()',1)[1].split('static std::wstring commandWaitReason()',1)[0]
else:
    link='static uint32_t commandMatch(){return snap.match;} static bool commandChannelReady(){return snap.commandStatus==1;}'
merge='static void mergeExternal(){'+main.split('static void mergeExternal(){',1)[1].split('static void sampleExternal(',1)[0]
program=r'''
#include "protocol.h"
#include "control_link.h"
#include "command_delivery.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include <cassert>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
using namespace assist;using DWORD=uint32_t;
static DWORD clockNow=100,lastData=100,attachAt=0;static DWORD GetTickCount(){return clockNow;}
static DWORD displayOffset=0;static DWORD sampleRenderClock(const Shared*){return clockNow+displayOffset;}
static auto storage=std::make_unique<Shared>();static Shared*shared=storage.get();
static Snapshot snap{},incoming{};static const Snapshot*externalData;
static std::unordered_map<uint32_t,int> labelRasters;
static struct {void clear(){}void update(const Snapshot&,int){}} motion;
static void rememberLiveTypes(){}
// Presentation is orthogonal to this command-identity/merge fixture.
static void clearEnemyDisplay(){}
static void updateEnemyDisplay(const Snapshot&){}
'''+link+merge+r'''
int main(){
 // Fresh local clock inside the production handshake timeout; caller's old
 // timestamp cannot underflow after ensureBridge. Real timeouts/wrap still work.
 attachAt=101;clockNow=101;lastData=0;assert(!uninitializedBridgeTimedOut());
 clockNow=8101;assert(!uninitializedBridgeTimedOut());clockNow=8102;assert(uninitializedBridgeTimedOut());
 attachAt=0xfffffff0u;clockNow=0x10;assert(!uninitializedBridgeTimedOut());
 clockNow=uint32_t(attachAt+8001);assert(uninitializedBridgeTimedOut());
 shared->request=1;assert(!uninitializedBridgeTimedOut());shared->ack=1;
 // A responding observer/menu bridge with no playable match is healthy.
 clockNow=20000;attachAt=100;lastData=19900;assert(!uninitializedBridgeTimedOut());
 clockNow=27900;assert(!uninitializedBridgeTimedOut());clockNow=27901;assert(uninitializedBridgeTimedOut());
 clockNow=100;lastData=100;attachAt=0;
 Snapshot d{};d.status=1;d.renderStatus=1;d.match=10;externalData=&d;
 incoming.status=1;incoming.bridgeStatus=1;incoming.commandStatus=1;incoming.match=77;snap=incoming;
 delivery::Queue q;assert(q.add({AutoBoard,0,0,123,77,100,0}));
 for(uint32_t time:{100u,1701u,3001u,7001u}){
  clockNow=time;d.sampledTick=time;++d.match;mergeExternal();
  if(snap.match!=77){std::cerr<<"command epoch replaced at "<<time<<"ms: "<<snap.match<<"\n";return 2;}
  assert(snap.status==1&&snap.commandStatus==1&&commandChannelReady());
  assert(q.expire(time,123,commandMatch(),true)==0&&q.size==1);
 }
 displayOffset=5;d.sampledTick=clockNow+5;mergeExternal();assert(snap.renderStatus==1);displayOffset=0;
 d.sampledTick=clockNow;d.renderStatus=0;mergeExternal();assert(snap.renderStatus==0&&commandMatch()==77&&commandChannelReady());
 d.renderStatus=1;mergeExternal();assert(snap.renderStatus==1&&q.size==1);
 d.status=0;mergeExternal();assert(snap.status==1&&commandChannelReady());
 incoming.commandStatus=0;snap=incoming;lastData=clockNow;mergeExternal();
 assert(!commandChannelReady()&&commandMatch()==77&&q.size==1);
 incoming.commandStatus=1;incoming.match=78;snap=incoming;mergeExternal();
 assert(commandChannelReady()&&commandMatch()==78);
 assert(q.expire(clockNow,123,commandMatch(),true)==1);
 incoming.status=-1;incoming.commandStatus=0;snap=incoming;d.status=1;d.sampledTick=clockNow;mergeExternal();
 assert(!commandChannelReady()&&snap.status==-1);
 std::cout<<"PASS shipped host: 1.6/2.9/6.9s callback delays do not change epoch, clear AutoBoard or disable hotkeys; real round change/fault still rejects\n";
}
'''
with tempfile.TemporaryDirectory(prefix='ra2-host-link-') as tmp:
    cpp=Path(tmp)/'host.cpp';exe=Path(tmp)/'host';cpp.write_text(program)
    subprocess.run(['g++','-std=c++17','-O2','-I',str(root/'assist/source'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
