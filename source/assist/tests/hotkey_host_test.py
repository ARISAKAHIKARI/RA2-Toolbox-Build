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
static uint32_t now=1000,scene=8;static uint32_t GetTickCount(){return now;}
static AdditionalSettings extra;static Snapshot snap{};static auto memory=std::make_unique<Shared>();static Shared*shared=memory.get();
static bool enabled=true,focused=true,hasSelection=false,ejectMode=false,hookEject=false,csMode=false,hookAutoCS=false;
static bool foregroundGame(){return focused;}static bool commandFresh(uint32_t){return true;}static uint32_t commandMatch(){return scene;}
static int doubleMs=350;static typekeys::Tap typeTap,variantTap;static hotkeyroute::Registry hotkeyRegistry;static hotkeyroute::Pending hotkeyPending;
static int repairCheck=1,ejectCheck=2,csCheck=3,hookChecks[4]={4,5,6,7};static std::map<int,int>controls;
static int SendMessageW(int id,int msg,int value,int){if(msg==BM_SETCHECK)controls[id]=value;return controls[id];}
struct Binding{std::string id;WORD key;};static std::vector<Binding>typeBindings,swBindings;
static int actions[]={Repair,CombatSelection,EmptyTransports,DamagedSelection,IdleHarvester,IdleEngineer,IdleTransport,ClearBeacons,RevealMap,SelectIron,SelectChrono,AutoBoard};
static unsigned notifications=0,saves=0;static void notify(const std::wstring&){++notifications;}static void save(){++saves;}
static struct{int size=0;}commandQueue;
static std::vector<int>commands;static std::vector<std::pair<int,bool>> selections;
static void send(int cmd,int=0){commands.push_back(cmd);++shared->request;}
static void sendSelection(int index,bool global,const uint8_t* =nullptr){selections.push_back({index,global});++shared->selectRequest;}
'''+'''static WORD hotkeys[12]{},csKey=0,advancedKey=0,unloadKey=0,ejectKey=0,csHoldKey=0,magnetronKey=0;\nstatic WORD comparableKey'''+conflict+body+r'''
static void ack(){shared->ack=shared->request;shared->selectAck=shared->selectRequest;serviceHotkeyBatch();}
int main(){
 extra.allHotkeys=false;hotkeys[0]=0x641;assert(keyConflict(0xe41,"",false));extra.allHotkeys=true;assert(!keyConflict(0x641,"",false));snap.status=1;snap.match=8;snap.typeCount=1;snap.types[0].kind=40;std::strcpy(snap.types[0].id,"TANK");typeBindings.push_back({"TANK",0x641});
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
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'host.cpp';exe=Path(d)/'host';p.write_text(program)
 subprocess.run(['g++','-std=c++17','-O2','-I',str(r/'assist/source'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: production hotkey dispatcher executes every match once, waits for command/selection ACK, preserves both double-tap selections, rejects stale bindings/context/deadlines; default-off unchanged')
