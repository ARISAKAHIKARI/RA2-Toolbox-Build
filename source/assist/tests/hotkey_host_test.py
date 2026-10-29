"""Execute the production WM_HOTKEY dispatcher with independent ACK channels."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[2]
body=(r/'assist/source/hotkey_host.h').read_text()
conflict=(r/'assist/source/type_bindings_ui.h').read_text().split('static WORD comparableKey',1)[1].split('static void syncTypeConfig()',1)[0]
program=r'''
#include "protocol.h"
#include "additional_settings.h"
#include "hotkey_routes.h"
#include "type_hotkey.h"
#include "view_zoom.h"
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <cassert>
using namespace assist;using WPARAM=uintptr_t;using LPARAM=intptr_t;using WORD=uint16_t;
constexpr int MOD_ALT=1,MOD_CONTROL=2,MOD_SHIFT=4,HOTKEYF_SHIFT=1,HOTKEYF_CONTROL=2,HOTKEYF_ALT=4;
constexpr int BM_SETCHECK=1,BM_GETCHECK=2,BST_CHECKED=1,BST_UNCHECKED=0;
static unsigned LOBYTE(uintptr_t x){return x&255;}static unsigned HIBYTE(uintptr_t x){return (x>>8)&255;}static unsigned LOWORD(uintptr_t x){return x&65535;}static unsigned HIWORD(uintptr_t x){return (x>>16)&65535;}
static WORD MAKEWORD(unsigned a,unsigned b){return WORD((a&255)|((b&255)<<8));}
struct ControlUpdate {};static int ui=0;constexpr int CB_SETCURSEL=3;static void SendDlgItemMessageW(int,int,int,int,int){}static void syncAdditional(){}
static uint32_t now=1000,scene=8;static uint32_t GetTickCount(){return now;}
static AdditionalSettings extra;static Snapshot snap{};static auto memory=std::make_unique<Shared>();static Shared*shared=memory.get();
static bool enabled=true,focused=true,hasSelection=false,ejectMode=false,hookEject=false,csMode=false,hookAutoCS=false;
static bool formationInputOwns(){return false;}static bool formationHotkey(WORD){return false;}namespace watchhost {static int pins=0;static uint32_t lastKey=0;static void pin(uint32_t key){lastKey=key;++pins;}}
static bool foregroundGame(){return focused;}static bool freshConnection=true;static bool commandFresh(uint32_t){return freshConnection;}static uint32_t commandMatch(){return scene;}
static int doubleMs=350;static typekeys::Tap typeTap,variantTap;static hotkeyroute::Registry hotkeyRegistry;static hotkeyroute::Pending hotkeyPending;
static int repairCheck=1,ejectCheck=2,csCheck=3,hookChecks[4]={4,5,6,7};static std::map<int,int>controls;
static int SendMessageW(int id,int msg,int value,int){if(msg==BM_SETCHECK)controls[id]=value;return controls[id];}
struct Binding{std::string id;WORD key;};static std::vector<Binding>typeBindings,swBindings;
static int actions[]={Repair,CombatSelection,EmptyTransports,DamagedSelection,IdleHarvester,IdleEngineer,IdleTransport,ClearBeacons,RevealMap,SelectIron,SelectChrono,AutoBoard};
static uint32_t pid=123;static bool externalData=true;static uint32_t sampleRenderClock(void*){return now;}
static struct {uint32_t idleAircraftTick=1000;int idleAircraftCount=3;uint32_t idleAircraftIDs[8192]={10,20,30};}externalSampler;
static std::vector<uint32_t> selectedAircraft;
static int hook=0;namespace gameinput {uint8_t keys[256]{};uint32_t serial[256]{};void keySnapshot(int,uint8_t*k,uint32_t*s){memcpy(k,keys,256);memcpy(s,serial,1024);}}
static short GetAsyncKeyState(unsigned vk){return gameinput::keys[vk]?-32768:0;}
static unsigned notifications=0,saves=0;static void notify(const std::wstring&){++notifications;}static void save(){++saves;}
static struct{int size=0;}commandQueue;
static std::vector<int>commands;static std::vector<std::pair<int,bool>> selections;
static void send(int cmd,int=0){commands.push_back(cmd);++shared->request;}
static void sendSelection(int index,bool global,const uint8_t* =nullptr,uint32_t object=0){if(index==-4)selectedAircraft.push_back(object);selections.push_back({index,global});++shared->selectRequest;}
'''+(r/'assist/source/idle_aircraft_selection.h').read_text()+'''static WORD socialKeys[9]{};static WORD idleAircraftKey=0x650;static WORD hotkeys[12]{},csKey=0,advancedKey=0,unloadKey=0,ejectKey=0,csHoldKey=0,magnetronKey=0;\nstatic WORD comparableKey'''+conflict+body+r'''
static void ack(){shared->ack=shared->request;shared->selectAck=shared->selectRequest;serviceHotkeyBatch();}
int main(){
 extra.allHotkeys=false;hotkeys[0]='G';assert(keyConflict('G'|0x3000,"",false));assert(keyConflict('G'|0x1000,"",false));assert(!keyConflict('G'|0x2000,"",false));
 extra.allHotkeys=false;hotkeys[0]=0x641;assert(keyConflict(0xe41,"",false));extra.allHotkeys=true;assert(!keyConflict(0x641,"",false));snap.status=1;snap.renderStatus=1;snap.match=8;snap.typeCount=1;snap.types[0].kind=40;std::strcpy(snap.types[0].id,"TANK");typeBindings.push_back({"TANK",0x641});
 auto install=[](int,uint16_t){return true;};for(int id:{1001,1100,1700,1699})assert(hotkeyRegistry.add(id,0x641,true,install));
 const intptr_t event=(65<<16)|(MOD_CONTROL|MOD_ALT);
 handleHotkey(1001,event);assert(commands.size()==1&&commands.back()==CombatSelection);assert(selections.empty()&&hotkeyPending.count==3);
 serviceHotkeyBatch();assert(selections.empty()); // command ACK gates the next action
 ack();assert(selections.size()==1&&selections.back()==std::make_pair(-3,false));
 serviceHotkeyBatch();assert(selections.size()==1); // selection ACK has its own gate
 ack();assert(selections.size()==2&&selections.back()==std::make_pair(-2,false));
 ack();assert(csMode&&saves==1&&!hotkeyPending.count);
 now+=100;handleHotkey(1001,event);ack();assert(selections.back()==std::make_pair(-3,true));ack();assert(selections.back()==std::make_pair(-2,true));ack();assert(!csMode);
 unsigned count=commands.size();handleHotkey(1001,event^(MOD_ALT));assert(commands.size()==count);handleHotkey(777,event);assert(commands.size()==count);
 extra.allHotkeys=false;handleHotkey(1001,event);assert(commands.size()==count+1&&!hotkeyPending.count);ack();
 extra.allHotkeys=true;handleHotkey(1001,event);focused=false;serviceHotkeyBatch();assert(!hotkeyPending.count);focused=true;ack();
 assert(hotkeyPending.add(*hotkeyRegistry.find(1001),now,8));++scene;serviceHotkeyBatch();assert(!hotkeyPending.count);
 --scene;assert(hotkeyPending.add(*hotkeyRegistry.find(1001),now,8));now+=2501;serviceHotkeyBatch();assert(!hotkeyPending.count);

 hotkeyRegistry.clear();extra.allHotkeys=false;assert(hotkeyRegistry.add(1710,0x641,false,install));
 handleHotkey(1710,event);assert(commands.back()==Cheer);ack();
 hotkeyRegistry.clear();assert(hotkeyRegistry.add(1718,0x641,false,install));handleHotkey(1718,event);assert(commands.back()==Taunt);ack();
 snap.chat=1;auto socialCount=commands.size();handleHotkey(1718,event);assert(commands.size()==socialCount);snap.chat=0;
 socialKeys[3]=0x641;assert(keyConflict(0x641,"",false));socialKeys[3]=0;
 hotkeyRegistry.clear();assert(hotkeyRegistry.add(1730,0x657,false,install));handleHotkey(1730,('W'<<16)|(MOD_CONTROL|MOD_ALT));assert(watchhost::pins==1);
 hotkeyRegistry.clear();extra.allHotkeys=false;extra.idleAircraft=false;snap.renderStatus=1;
 assert(keyConflict(idleAircraftKey,"",false));
 assert(hotkeyRegistry.add(1701,idleAircraftKey,false,install));const intptr_t planeEvent=('P'<<16)|(MOD_CONTROL|MOD_ALT);
 externalSampler.idleAircraftTick=now;
 handleHotkey(1701,planeEvent);ack();assert(selectedAircraft.back()==10);
 selectNextIdleAircraft();ack();assert(selectedAircraft.back()==20); // button uses the same cursor
 handleHotkey(1701,planeEvent);ack();assert(selectedAircraft.back()==30);
 handleHotkey(1701,planeEvent);ack();assert(selectedAircraft.back()==10);
 externalSampler.idleAircraftCount=2;externalSampler.idleAircraftIDs[1]=30;handleHotkey(1701,planeEvent);ack();assert(selectedAircraft.back()==30);
 ++pid;handleHotkey(1701,planeEvent);ack();assert(selectedAircraft.back()==10);
 ++scene;handleHotkey(1701,planeEvent);ack();assert(selectedAircraft.back()==10);
 auto selected=selectedAircraft.size();snap.chat=1;handleHotkey(1701,planeEvent);snap.chat=0;assert(selectedAircraft.size()==selected);
 snap.renderStatus=0;handleHotkey(1701,planeEvent);snap.renderStatus=1;assert(selectedAircraft.size()==selected);
 focused=false;handleHotkey(1701,planeEvent);focused=true;assert(selectedAircraft.size()==selected);
 now+=501;handleHotkey(1701,planeEvent);assert(selectedAircraft.size()==selected);externalSampler.idleAircraftTick=now;
 externalSampler.idleAircraftCount=0;handleHotkey(1701,planeEvent);assert(selectedAircraft.size()==selected);

 // Sticky uses the same configurable/double-only routing and command channel.
 hotkeyRegistry.clear();hotkeyPending.clear();assert(hotkeyRegistry.add(1723,0x8000|'S',false,install));auto stickyCount=commands.size();handleHotkey(1723,'S'<<16);assert(commands.size()==stickyCount);now+=100;handleHotkey(1723,'S'<<16);assert(commands.size()==stickyCount+1&&commands.back()==StickyGuard);ack();
 // An external display can be ready while the command bridge has no identity.
 hotkeyRegistry.clear();hotkeyPending.clear();assert(hotkeyRegistry.add(1723,'S',false,install));auto oldScene=scene;scene=0;now+=3000;auto beforeNotice=notifications;auto beforeCommands=commands.size();
 for(int n=0;n<140;++n)handleHotkey(1723,'S'<<16);
 assert(hotkeyPending.count==0&&commands.size()==beforeCommands&&notifications==beforeNotice+1);scene=oldScene;
 freshConnection=false;now+=3000;handleHotkey(1723,'S'<<16);assert(hotkeyPending.count==0);freshConnection=true;
 // Expiration must run even while either mailbox is awaiting acknowledgement.
 ++shared->request;for(int n=0;n<128;++n)handleHotkey(1723,'S'<<16);assert(hotkeyPending.count==128);now+=2501;serviceHotkeyBatch();assert(hotkeyPending.count==0);ack();handleHotkey(1723,'S'<<16);assert(commands.size()==beforeCommands+1);ack();
 ++shared->selectRequest;handleHotkey(1723,'S'<<16);assert(hotkeyPending.count==1);now+=2501;serviceHotkeyBatch();assert(hotkeyPending.count==0);ack();
 // Configured double-only commands execute once, after the second tap.
 hotkeyRegistry.clear();hotkeyPending.clear();assert(hotkeyRegistry.add(1730,0x8000|'Q',false,install));
 auto pins=watchhost::pins;handleHotkey(1730,'Q'<<16);assert(watchhost::pins==pins);now+=100;handleHotkey(1730,'Q'<<16);assert(watchhost::pins==pins+1);
 // Bare modifiers use physical transitions, never held-key timer repeats.
 hotkeyRegistry.clear();assert(hotkeyRegistry.add(1730,0x8000|17,false,install));
 hotkeyRegistry.synchronize(gameinput::keys,scene);
 gameinput::keys[17]=1;gameinput::serial[17]=1;serviceHotkeyBatch();serviceHotkeyBatch();assert(watchhost::pins==pins+1);
 gameinput::keys[17]=0;serviceHotkeyBatch();now+=100;gameinput::keys[17]=3;gameinput::serial[17]=2;serviceHotkeyBatch();assert(watchhost::pins==pins+2);
 // A pending first tap is discarded on focus loss.
 gameinput::keys[17]=0;serviceHotkeyBatch();gameinput::keys[17]=1;gameinput::serial[17]=3;now+=50;serviceHotkeyBatch();focused=false;serviceHotkeyBatch();focused=true;
 gameinput::serial[17]=4;now+=100;serviceHotkeyBatch();assert(watchhost::pins==pins+2);
 gameinput::keys[17]=0;serviceHotkeyBatch();gameinput::keys[17]=1;gameinput::serial[17]=5;now+=100;serviceHotkeyBatch();assert(watchhost::pins==pins+2);
 gameinput::keys[17]=0;serviceHotkeyBatch();gameinput::keys[17]=1;now+=100;serviceHotkeyBatch();assert(watchhost::pins==pins+3);

 // The WM_HOTKEY dispatcher must not execute a release action at key down.
 hotkeyRegistry.clear();hotkeyPending.clear();memset(gameinput::keys,0,256);assert(hotkeyRegistry.add(1723,'X'|0x200|0x2000,false,install));hotkeyRegistry.synchronize(gameinput::keys,scene);
 auto releaseCount=commands.size();gameinput::keys[17]=gameinput::keys['X']=1;
 handleHotkey(1723,('X'<<16)|MOD_CONTROL);assert(commands.size()==releaseCount);serviceHotkeyBatch();assert(commands.size()==releaseCount);
 gameinput::keys['X']=0;serviceHotkeyBatch();assert(commands.size()==releaseCount+1&&commands.back()==StickyGuard);ack();
 gameinput::keys['X']=1;handleHotkey(1723,('X'<<16)|MOD_CONTROL);gameinput::keys[17]=0;serviceHotkeyBatch();gameinput::keys[17]=1;serviceHotkeyBatch();gameinput::keys['X']=0;serviceHotkeyBatch();assert(commands.size()==releaseCount+1);
 hotkeyRegistry.clear();memset(gameinput::keys,0,256);assert(hotkeyRegistry.add(1723,'Q'|0xa000,false,install));hotkeyRegistry.synchronize(gameinput::keys,scene);
 gameinput::keys['Q']=1;handleHotkey(1723,'Q'<<16);gameinput::keys['Q']=0;serviceHotkeyBatch();now+=100;
 gameinput::keys['Q']=1;handleHotkey(1723,'Q'<<16);assert(commands.size()==releaseCount+1);gameinput::keys['Q']=0;serviceHotkeyBatch();assert(commands.size()==releaseCount+2);ack();
 // Mixed legacy/explicit Press type groups have one stable interpretation,
 // regardless of the catalogue row that owns the OS registration.
 typeBindings.push_back({"PLANE",WORD(0x1641)});extra.allHotkeys=false;
 for(bool reversed:{false,true}){
  if(reversed)std::reverse(typeBindings.begin(),typeBindings.end());
  hotkeyRegistry.clear();assert(hotkeyRegistry.add(1100,typeBindings[0].key,false,install));
  now+=1000;handleHotkey(1100,event);assert(!selections.back().second);ack();
  now+=100;handleHotkey(1100,event);assert(!selections.back().second);ack();
 }

}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'host.cpp';exe=Path(d)/'host';p.write_text(program)
 subprocess.run(['g++','-std=c++17','-O2','-I',str(r/'assist/source'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: disconnected command identity does not enter or falsely fill queue; notification throttled; stale command/selection ACK waits expire and recovery dispatches once')
print('PASS: idle-aircraft hotkey and button share cycle, wrap and process/match reset; hidden indicator independence; stale/menu/chat/focus rejection')
print('PASS: production hotkey dispatcher executes every match once, waits for command/selection ACK, preserves both double-tap selections, rejects stale bindings/context/deadlines; default-off unchanged')
