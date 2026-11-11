"""Compile extracted production gates with controlled boundaries; generate x86 target include.
No game/network/Windows-UI outcome is inferred from this test.
"""
import argparse, json, subprocess
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--source',type=Path,default=Path(__file__).parents[1]/'source');a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
def function(text,signature):
    start=text.index(signature);opening=text.index('{',start);depth=1;i=opening+1
    while depth:
        depth+=(text[i]=='{')-(text[i]=='}');i+=1
    return text[start:i]
source=(a.source/'network_sender_bridge.h').read_text()
delivery='static bool networkSenderDeliveryReady()'
parts=[]
if delivery in source:parts.append(function(source,delivery))
parts.append(function(source,'static bool networkHousePreserveReady()'))
if delivery not in source:parts.append('static bool networkSenderDeliveryReady(){return networkHousePreserveReady();}')
parts.append(function(source,'extern "C" int __cdecl AssistNetworkSend('))
prefix=r'''
#include <cstdint>
#include <cassert>
#include <cstdio>
#define __cdecl
using DWORD=uint32_t;
namespace activation {enum {Ready=1,DisplayHidden=6};}
struct Shared{bool stop=false,networkHousePreserve=true;int controlAll=1;unsigned heartbeat=1000,networkHousePackets=0;} data,*sh=&data;
struct {int commandStatus=1,status=1,session=3;unsigned match=1;}snap;
struct {bool active=false;}readOnlyState;
bool networkSenderEnabled=true,networkSenderInstalled=true,networkSenderFaulted=false,networkSenderPacking=false,networkSenderCacheDirty=false,fault=false;
unsigned match=1,now=1000;int networkSenderState=0;
bool world=true,sites=true,allowed=true,transition=false,running=true,context=true,foreign=true,packed=false;
int protocol=1,session=3,dropped=0;
namespace controlmode{bool sender(int n){return n==1;}}
namespace readonly{bool commands(Shared*){return allowed;}}
namespace eventdelivery{bool isMultiplayer(int n){return n==3||n==4;}}
namespace networksender{enum{Blocked=-1};bool protocol(int n){return n>=0&&n<=2;}}
namespace runtimeunwind{template<class A,class B>void run(A a,B b){a();b();}}
bool controlTransitionPending(){return transition;}
unsigned sampleClock(Shared*){return now;}
template<class T>T get(unsigned at){return T(at==0xA8E9A0?running:at==0xA8B238?session:at==0xA8B24C?protocol:0);}
bool networkSenderWorld(){return world;}bool networkSenderOwnSites(){return sites;}
bool networkSenderContext(){return context;}void networkSenderPublish(){}
bool networkSenderClearCache(){networkSenderCacheDirty=false;return true;}
bool networkSenderNoForeign(){return !foreign;}
bool networkSenderCancelForeign(bool invalidOnly=false){if(!invalidOnly&&foreign){foreign=false;++dropped;}return true;}
int networkSenderOriginal(unsigned,unsigned,int,int,int){packed=networkSenderPacking;return 1;}
'''
test=r'''
int main(){
 assert(networkHousePreserveReady()&&networkSenderDeliveryReady());
 snap.commandStatus=activation::DisplayHidden;
 assert(!networkHousePreserveReady()&&networkSenderDeliveryReady());
 assert(AssistNetworkSend(0,0,100,15,0)==1&&packed&&foreign&&dropped==0);
 assert(!networkSenderPacking);
 snap.commandStatus=99;assert(!networkSenderDeliveryReady());snap.commandStatus=activation::DisplayHidden;
 now=4000;assert(!networkSenderDeliveryReady());now=1000;
 sh->stop=true;assert(!networkSenderDeliveryReady());sh->stop=false;
 allowed=false;assert(!networkSenderDeliveryReady());allowed=true;
 readOnlyState.active=true;assert(!networkSenderDeliveryReady());readOnlyState.active=false;
 world=false;assert(!networkSenderDeliveryReady());world=true;
 sites=false;assert(!networkSenderDeliveryReady());sites=true;
 transition=true;assert(!networkSenderDeliveryReady());transition=false;
 protocol=99;assert(!networkSenderDeliveryReady());protocol=1;
 session=0;assert(!networkSenderDeliveryReady());session=3;
 running=false;assert(!networkSenderDeliveryReady());running=true;
 networkSenderFaulted=true;assert(!networkSenderDeliveryReady());networkSenderFaulted=false;
 snap.match=2;assert(!networkSenderDeliveryReady());snap.match=1;
 sh->networkHousePreserve=false;assert(!networkSenderDeliveryReady());
 AssistNetworkSend(0,0,100,15,0);assert(!packed&&!foreign&&dropped==1);
 puts("PASS hidden delivery, visible-only admission and 14 blocking boundaries");
}
'''
cpp=a.output/'network_delivery.cpp';cpp.write_text(prefix+'\n'.join(parts)+test)
subprocess.run(['g++','-std=c++17','-O2',str(cpp),'-o',str(a.output/'network_delivery')],check=True)
subprocess.run([str(a.output/'network_delivery')],check=True)
target=function((a.source/'player_test_bridge.h').read_text(),'static int destructPlayerTarget(')
(a.output/'destruct_target.inc').write_text(target+'\n')
(a.output/'gate-results.json').write_text(json.dumps({'hidden_delivery':True,'visible_only_admission':True,'blocking_boundaries':14,'live_platform_tested':False},indent=2)+'\n')
