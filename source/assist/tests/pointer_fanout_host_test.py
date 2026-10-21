"""Run the shipped host fanout service with a Win32/read-session fixture."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[2]
body=(r/'assist/source/pointer_host.h').read_text()
program=r'''
#include "protocol.h"
#include "additional_settings.h"
#include "pointer_batch.h"
#include <map>
#include <memory>
#include <string>
#include <cassert>
#include <algorithm>
#include <cstring>
using namespace assist;using DWORD=uint32_t;using LONG=int32_t;
struct RECT{int left,top,right,bottom;};struct POINT{int x,y;};
constexpr int VK_SHIFT=16,VK_CONTROL=17,VK_MENU=18;
static DWORD clockNow=1000,cancelSerial=3;static DWORD GetTickCount(){return clockNow;}
static bool focused=true,visible=true,fresh=true;static int game=1,hook=2;
static auto storage=std::make_unique<Shared>();static Shared*shared=storage.get();
static Snapshot snap{},incoming{};static AdditionalSettings extra;static pointerinput::PendingMatches pointerMatches;
static uint32_t settingsRevision=1,cooperativeDelivered=0;static bool retryCooperative=false;
static uint32_t advancedKey=0,magnetronKey=0,csHoldKey=0;static unsigned wakes=0,notifications=0;
static bool hookSmartUnload=false;static uint32_t unloadKey=0;
#define LOBYTE(v) ((v)&255)
static uint32_t featureFlags=AdvancedInput|CustomCS;static uint32_t flags(){return featureFlags;}
static uint32_t commandMatch(){return 8;}static bool commandFresh(DWORD){return fresh;}
static bool foregroundGame(){return focused;}static bool GetClientRect(int,RECT*r){*r={0,0,800,600};return true;}
static void MemoryBarrier(){}template<class T>static void InterlockedIncrement(volatile T*p){++*p;}
static void wakeGame(){++wakes;}static void notify(const std::wstring&){++notifications;}
namespace external{namespace display{struct State{bool visible()const{return ::visible;}};template<class R>State inspect(R){return {};}}}
static std::map<uint32_t,uint32_t> memory;
static struct{bool readFresh(uint32_t at,void*out,size_t n){auto i=memory.find(at);if(i==memory.end()||n!=4)return false;std::memcpy(out,&i->second,4);return true;}}externalReader;
namespace gameinput{
struct PointerPress{DWORD tick=0,serial=0;uint32_t match=0;int operation=0;POINT point{};int width=0,height=0;uint8_t keys[256]{};};
static PointerPress event;static bool queued=false;
static void setPointerContext(int,uint32_t,bool){}static uint32_t pointerCancelSerial(int){return cancelSerial;}
static bool takePointer(int,PointerPress&e,DWORD&dropped){dropped=0;if(!queued)return false;e=event;queued=false;return true;}
}
namespace buildreserve{static bool nativePointer(const gameinput::PointerPress&){return false;}}
namespace airway{static bool aircraft=false;static bool native(){return airpathpolicy::native(snap.session,extra.nativeAirWaypoints);}static bool active(){return airpathpolicy::events(snap.session,extra.airWaypoints,extra.nativeAirWaypoints);}static void capture(const pointerinput::Intent&,const pointerinput::Mailbox&){} static bool ended=false;static bool selectedAircraft(){return aircraft;}static void manual(const gameinput::PointerPress&){} }
'''+body+r'''
static void capture(){gameinput::event={};auto&e=gameinput::event;e.tick=clockNow;e.serial=cancelSerial;e.match=8;e.operation=1;e.point={100,100};e.width=800;e.height=600;gameinput::queued=true;}
static void ack(int accepted=1){shared->pointer.ack=shared->pointer.request;shared->pointer.result=1;shared->pointer.accepted=accepted;shared->pointer.rejected=0;}
static void initialize(){
 pointerMatches.clear();extra.allHotkeys=true;for(auto&b:extra.mouseButtons)b=0;extra.mouseButtons[pointerinput::MouseAdvanced]=extra.mouseButtons[pointerinput::MouseCS]=1;
 memory={{0xA83D4C,0x10000},{0xA8ED84,5},{0x887324,0x20000},{0x200B0,0},{0x200B4,0},{0xA8EB84,800},{0xA8EB88,600},{0xA8ECB8+16,1},{0xA8ECB8+4,0x30000},{0x30000,0x40000},{0x40010,10},{0x4021C,0x10000}};
 focused=visible=fresh=true;retryCooperative=false;shared->cooperativeRequest.serial=shared->cooperativePlan=shared->cooperativeAck=cooperativeDelivered=0;clockNow+=10;
}
int main(){
 initialize();capture();gameinput::event.operation=5;gameinput::event.keys['D']=gameinput::event.keys[VK_CONTROL]=1;
 hookSmartUnload=true;unloadKey='D'|(2<<8);unsigned beforeUnload=shared->pointer.request;
 servicePointer();assert(shared->pointer.request==beforeUnload);
 capture();gameinput::event.operation=5;gameinput::event.keys['D']=1;servicePointer();
 assert(shared->pointer.request==beforeUnload+1&&shared->pointer.intent.operation==pointerinput::Cancel);ack();servicePointer();hookSmartUnload=false;
 // C4 uses the existing mailbox, default chord and per-feature mouse setting.
 initialize();featureFlags=0;extra.c4Test=true;extra.c4Key='C'|(4<<8);extra.c4Mouse=1;
 capture();servicePointer();assert(shared->pointer.request==shared->pointer.ack); // no chord
 capture();gameinput::event.keys[18]=gameinput::event.keys['C']=1;servicePointer();assert(shared->pointer.intent.operation==pointerinput::C4Test&&shared->pointer.intent.count==1);ack();servicePointer();
 extra.c4Key=0;extra.c4Mouse=6;capture();gameinput::event.operation=7;unsigned prior=shared->pointer.request;servicePointer();assert(shared->pointer.request==prior);
 capture();gameinput::event.operation=6;servicePointer();assert(shared->pointer.request==prior+1&&shared->pointer.intent.operation==pointerinput::C4Test&&(shared->pointer.intent.bindings&pointerinput::LeftClick));ack();servicePointer();
 extra.c4Test=false;featureFlags=AdvancedInput|CustomCS;

 initialize();capture();servicePointer();auto first=shared->pointer.intent;assert(first.bindings&pointerinput::BatchStart);assert((first.bindings&7)==pointerinput::Advanced);
 ack();shared->cooperativeRequest.serial=9;unsigned request=shared->pointer.request;clockNow+=200;servicePointer();assert(shared->pointer.request==request); // not overwritten before host consumes it
 cooperativeDelivered=9;shared->cooperativePlan=1;servicePointer();assert(shared->pointer.request==request);
 shared->cooperativeAck=1;retryCooperative=true;servicePointer();assert(shared->pointer.request==request);
 retryCooperative=false;servicePointer();assert(shared->pointer.request==request+1);auto second=shared->pointer.intent;
 assert(second.bindings&pointerinput::BatchNext);assert((second.bindings&7)==pointerinput::CS);assert(pointerinput::sameCapture(first,second));assert(second.tick==first.tick); // no timestamp renewal
 ack();servicePointer();assert(!pointerMatches.active());
 initialize();capture();servicePointer();ack();++cancelSerial;request=shared->pointer.request;servicePointer();assert(!pointerMatches.active()&&shared->pointer.request==request);
 initialize();capture();servicePointer();ack();focused=false;request=shared->pointer.request;servicePointer();assert(!pointerMatches.active()&&shared->pointer.request==request);
 initialize();capture();servicePointer();ack();++settingsRevision;request=shared->pointer.request;servicePointer();assert(!pointerMatches.active()&&shared->pointer.request==request);
 initialize();capture();servicePointer();ack();clockNow+=1501;request=shared->pointer.request;servicePointer();assert(!pointerMatches.active()&&shared->pointer.request==request);
 initialize();extra.allHotkeys=false;capture();servicePointer();assert(!(shared->pointer.intent.bindings&(pointerinput::BatchStart|pointerinput::BatchNext)));assert((shared->pointer.intent.bindings&7)==(pointerinput::Advanced|pointerinput::CS));ack();servicePointer();
 // A sale remains eligible without a selection when another matched route needs one.
 initialize();featureFlags=AdvancedInput|SellVehicles;extra.mouseButtons[pointerinput::MouseCS]=0;extra.mouseButtons[pointerinput::MouseSellVehicle]=1;extra.sellKey=0;memory[0xA8ECB8+16]=0;
 capture();servicePointer();assert(shared->pointer.intent.count==0&&pointerMatches.active());ack(0);servicePointer();assert(shared->pointer.intent.operation==pointerinput::Sell);
 // Native right release still reaches Runtime with automation disabled.
 ack(0);servicePointer();initialize();featureFlags=0;
 capture();gameinput::event.operation=4;servicePointer();assert(shared->pointer.intent.operation==pointerinput::FollowNative);ack();servicePointer();
 capture();gameinput::event.operation=4;gameinput::event.keys[VK_CONTROL]=1;servicePointer();assert(shared->pointer.intent.bindings&pointerinput::NativeForced);ack();servicePointer();
 // The same saved native preference uses native SP input and event MP input.
 extra.nativeAirWaypoints=true;airway::aircraft=true;snap.session=5;
 capture();gameinput::event.operation=4;gameinput::event.keys['Z']=1;request=shared->pointer.request;servicePointer();assert(shared->pointer.request==request);
 snap.session=3;capture();gameinput::event.operation=4;gameinput::event.keys['Z']=1;servicePointer();assert(shared->pointer.request==request+1&&shared->pointer.intent.operation==pointerinput::AirWaypoint);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'host.cpp';exe=Path(d)/'host';p.write_text(program)
 subprocess.run(['g++','-std=c++17','-O2','-I',str(r/'assist/source'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: production pointer host serializes all matches, waits for cooperative consumption/ACK/retries, preserves capture/deadline, cancels on context/config/manual change; default-off and no-selection sale')
