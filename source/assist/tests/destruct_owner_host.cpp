#ifdef NDEBUG
#undef NDEBUG
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "../source/protocol.h"
#include "../source/additional_settings.h"
#include "../source/command_delivery.h"
#include "../source/reveal_permission.h"
#include "../source/hotkey_routes.h"
#include "../source/destruct_owner_input.h"
#include "../source/placement_event_input.h"
#include "../source/runtime_feature_demand.h"
#include "../../suite/source/settings_categories.h"
using namespace assist;
static HWND ui=HWND(1),game=HWND(2);static Shared storage,*shared=&storage;
static Snapshot snap,incoming;static AdditionalSettings extra;static delivery::Queue commandQueue;
static hotkeyroute::Pending hotkeyPending;static hotkeyroute::Registry hotkeyRegistry;
static struct {void clear(){}}typeTap,variantTap;
static bool enabled=true,fresh=true,focus=true,window=true,hasSelection=false,hookSmartUnload=false;
static bool middle=false,forming=false;static uint32_t pid=123,epoch=7,ack=0,inFlightMatch=0;
static DWORD fakeNow=1000,messageAt=1000,lastData=1000,lastFind=0,sentAt=0;static int inFlightRetries=0;
static int techImplementation=0,techProfile=0,wakes=0,ensures=0,serviceCalls=0,registerCalls=0,settingsChanges=0,checks=0;
static std::wstring destructPendingTarget;static std::vector<std::wstring>notices;static std::function<void()>onSnapshot;
static WORD hotkeys[12]{},idleAircraftKey=0,csKey=0,advancedKey=0,unloadKey=0,ejectKey=0,csHoldKey=0,magnetronKey=0,socialKeys[9]{};
static struct Binding {WORD key=0;} binding;static std::vector<Binding>swBindings,typeBindings;
static std::map<std::wstring,int>ini;static std::map<int,LRESULT>controlValues;static std::map<int,std::wstring>feedback;
static DWORD testTick(){return fakeNow;}static LONG testMessageTime(){return LONG(messageAt);}static BOOL testWindow(HWND){return window;}
static HWND testItem(HWND,int id){return HWND(INT_PTR(id));}static LRESULT testMessage(HWND h,UINT msg,WPARAM wp,LPARAM){int id=int(INT_PTR(h));if(msg==HKM_SETHOTKEY){controlValues[id]=wp;return 0;}return controlValues[id];}
#define GetTickCount testTick
#define GetMessageTime testMessageTime
#define IsWindow testWindow
#define GetDlgItem testItem
#define SendMessageW testMessage
static bool commandFresh(DWORD){return fresh;}static bool commandFocus(){return focus;}static bool foregroundGame(){return focus;}
static uint32_t commandMatch(){return epoch;}static void readSnapshot(){if(onSnapshot)onSnapshot();}
static DWORD discoverGamePid(){return pid;}static void ensureBridge(){++ensures;}static void wakeGame(){++wakes;}
static bool techAllowed(const Type&){return false;}
static void constructionResult(int,int,uint32_t,uint32_t){}static void logCommandWait(){}
static void diag(const std::wstring&){}static void notify(const std::wstring&s){notices.push_back(s);}
static std::wstring commandWaitReason(){return L"fixture disconnected";}
static void setConsoleFeedback(HWND h,const std::wstring&s){feedback[int(INT_PTR(h))]=s;}
static void submitPlacementEvent(){assert(false&&"destruct fixture never submits placement");}
static void serviceHotkeyBatch(){++serviceCalls;}static bool runtimeMiddleOwnsNow(){return middle;}static bool formationHotkey(WORD){return forming;}
static void registerKeys(){++registerCalls;}static void changedSetting(){++settingsChanges;}static void updatePlayerTestUI(bool){}
static int value(const wchar_t*s,const wchar_t*k,int fallback){auto f=ini.find(std::wstring(s)+L"/"+k);return f==ini.end()?fallback:f->second;}
static void write(const wchar_t*s,const wchar_t*k,int v){ini[std::wstring(s)+L"/"+k]=v;}
struct ControlUpdate{ControlUpdate(){}~ControlUpdate(){}};
static bool destructOwnerKeyConflict(WORD key);
#include DESTRUCT_HOST_ROUTES
static void check(bool okay){assert(okay);++checks;}
static void setup(){
 storage={};shared=&storage;snap={};incoming={};extra={};extra.destructSelectedOwner=true;extra.destructSelectedOwnerKey='Q'|(6<<8);
 enabled=fresh=focus=window=true;hasSelection=middle=forming=false;pid=123;epoch=7;ack=0;fakeNow=messageAt=lastData=1000;wakes=ensures=serviceCalls=0;onSnapshot={};
 snap.status=snap.renderStatus=incoming.status=incoming.commandStatus=1;snap.match=incoming.match=epoch;
 commandQueue.clear();hotkeyPending.clear();hotkeyRegistry.clear();notices.clear();feedback.clear();controlValues.clear();
 swBindings.clear();typeBindings.clear();std::fill(std::begin(hotkeys),std::end(hotkeys),0);std::fill(std::begin(socialKeys),std::end(socialKeys),0);
}
static hotkeyroute::Group group(){hotkeyroute::Group g;g.actions={destructownerhost::Hotkey};g.bindings={WORD(extra.destructSelectedOwnerKey)};return g;}
static LPARAM keyEvent(){return MAKELPARAM(MOD_CONTROL|MOD_ALT,'Q');}
static void registerFixtureKey(){check(hotkeyRegistry.add(destructownerhost::Hotkey,WORD(extra.destructSelectedOwnerKey),true,[](int,WORD){return true;}));}
int main(){
 setup();submitDestructSelectedOwner();check(wakes==1&&shared->request==1&&shared->command==DestructPlayer);check(shared->arg==0&&shared->arg2==0&&shared->requestToken==0&&shared->requestMatch==7);check(commandQueue.size==0&&hotkeyPending.count==0);
 submitDestructSelectedOwner();check(wakes==1&&shared->request==1);shared->ack=shared->request;ack=shared->ack;check(wakes==1); // ACK does not replay.
 for(int reason=0;reason<18;++reason){setup();switch(reason){
  case 0:extra.destructSelectedOwner=false;break;case 1:enabled=false;break;case 2:shared=nullptr;break;case 3:game=nullptr;break;case 4:pid=0;break;case 5:window=false;break;case 6:fresh=false;break;case 7:focus=false;break;case 8:incoming.chat=1;break;case 9:shared->request=1;break;case 10:ack=1;break;case 11:commandQueue.size=1;break;case 12:hasSelection=true;break;case 13:shared->selectRequest=1;break;case 14:shared->pointer.request=1;break;case 15:shared->viewZoomBusy=1;break;case 16:snap.status=-1;break;case 17:epoch=0;break;}
  submitDestructSelectedOwner();check(wakes==0&&ensures==0&&hotkeyPending.count==0);if(reason!=11)check(commandQueue.size==0);game=HWND(2);
 }
 for(int reason=0;reason<7;++reason){setup();onSnapshot=[=]{switch(reason){case 0:epoch=8;break;case 1:pid=124;break;case 2:fakeNow+=151;break;case 3:incoming.status=0;break;case 4:incoming.chat=1;break;case 5:fresh=false;break;case 6:shared=nullptr;break;}};submitDestructSelectedOwner();check(wakes==0&&commandQueue.size==0);}
 for(int reason=0;reason<5;++reason){setup();send(DestructPlayer,reason==0?1:0,reason==1?1:0,reason==2?1:0,reason==3,reason==4?1:0);check(wakes==0&&commandQueue.size==0);}
 setup();check(!queueCommand(DestructPlayer,0,0,epoch)&&commandQueue.size==0);
 setup();check(dispatchHotkeyGroup(group())&&wakes==1&&hotkeyPending.count==0);shared->ack=shared->request;ack=shared->ack;check(wakes==1&&hotkeyPending.count==0);
 setup();commandQueue.size=1;check(dispatchHotkeyGroup(group())&&wakes==0&&hotkeyPending.count==0);commandQueue.clear();check(wakes==0);
 setup();auto mixed=group();mixed.actions.push_back(1001);mixed.bindings.push_back('Q');check(dispatchHotkeyGroup(mixed)&&wakes==0&&hotkeyPending.count==0);
 setup();registerFixtureKey();handleHotkey(destructownerhost::Hotkey,keyEvent());check(wakes==1&&hotkeyPending.count==0&&serviceCalls==1);
 // Holding the key creates no sampled autorepeat and never adds a second
 // destruction after Runtime's ACK arrives.
 shared->ack=shared->request;ack=shared->ack;uint8_t down[256]{};down['Q']=down[VK_CONTROL]=down[VK_MENU]=1;
 for(int n=0;n<50;++n){++fakeNow;hotkeyRegistry.sample(down,fakeNow,epoch,[](hotkeyroute::Group g){dispatchHotkeyGroup(std::move(g));});}check(wakes==1&&hotkeyPending.count==0);
 setup();registerFixtureKey();messageAt=849;handleHotkey(destructownerhost::Hotkey,keyEvent());check(wakes==0&&hotkeyPending.count==0);
 setup();registerFixtureKey();auto&g=hotkeyRegistry.groups[0];g.waiting=true;g.match=epoch;g.at=1;fixtureExpired();check(wakes==0&&hotkeyPending.count==0);
 setup();fixtureQueued(destructownerhost::Hotkey);check(wakes==0&&hotkeyPending.count==0);
 setup();hotkeyRegistry.add(1001,WORD(extra.destructSelectedOwnerKey),true,[](int,WORD){return true;});check(!destructownerhost::exclusive(hotkeyRegistry,extra.destructSelectedOwnerKey));check(destructownerhost::exclusive(hotkeyRegistry,'J'|(6<<8)));
 setup();extra.allHotkeys=true;check(keyConflict(WORD(extra.destructSelectedOwnerKey),"",false));controlValues[9254]=extra.placementEventKey;const auto prior=extra.destructSelectedOwnerKey;check(fixtureUI(9254,EN_CHANGE));check(extra.destructSelectedOwnerKey==prior&&controlValues[9254]==prior&&extra.allHotkeys);
 setup();hotkeys[0]=WORD(extra.destructSelectedOwnerKey)|uint16_t(keygesture::Mode::Release);check(destructOwnerKeyConflict(WORD(extra.destructSelectedOwnerKey)));check(dispatchHotkeyGroup(group())&&wakes==0&&hotkeyPending.count==0);extra.allHotkeys=true;
 controlValues[9254]=extra.placementEventKey;const auto savedKey=extra.destructSelectedOwnerKey;check(fixtureUI(9254,EN_CHANGE)&&extra.destructSelectedOwnerKey==savedKey);hotkeys[0]=0;
 controlValues[9254]='J'|(6<<8);check(fixtureUI(9254,EN_CHANGE)&&extra.destructSelectedOwnerKey==('J'|(6<<8)));controlValues[9253]=BST_UNCHECKED;check(fixtureUI(9253,BN_CLICKED)&&!extra.destructSelectedOwner);
 ini.clear();extra={};fixtureLoad();check(!extra.destructSelectedOwner&&!extra.destructSelectedOwnerKey);extra.destructSelectedOwner=true;extra.destructSelectedOwnerKey='K'|(2<<8);fixtureSave();extra={};fixtureLoad();check(extra.destructSelectedOwner&&extra.destructSelectedOwnerKey==('K'|(2<<8)));
 extra={};check(!runtimeFeatureDemand(0,extra,false,false));extra.destructSelectedOwner=true;check(runtimeFeatureDemand(0,extra,false,false));
 for(int id:{9253,9254,9185,9186})check(suite::categories::row(4,&id,1)==suite::categories::Experimental);int id=780;check(suite::categories::row(2,&id,1)==suite::categories::Rules);
 printf("{\"checks\":%d,\"passed\":true,\"scope\":\"actual Host send, immediate input, fresh WM_HOTKEY, delayed/stale/held-key rejection, UI and INI routes; simulated Win32\"}\n",checks);
}
