#ifdef NDEBUG
#undef NDEBUG
#endif
#include <windows.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "../source/protocol.h"
#include "../source/additional_settings.h"
#include "../source/settings_dependencies.h"
#include "../source/hotkey_routes.h"
#include "../source/runtime_feature_demand.h"
#include "../../suite/source/settings_categories.h"
using namespace assist;
static HWND ui=HWND(1),game=HWND(2);
static Snapshot snap,incoming;static Shared storage;static Shared*shared=&storage;
static AdditionalSettings extra;static bool ready=true,enabled=true,buildAnywhereMode=true,focus=true,visible=true,window=true,formed=false,hasSelection=false,techMode=false;
static int techImplementation=0,wakes=0,reads=0,saved=0,registered=0,settingsChanged=0,synced=0,filled=0,checks=0;
static uint32_t pid=123,epoch=7,ack=0,inFlightMatch=0;static int inFlightRetries=0;static DWORD fakeNow=1000,lastData=1000,sentAt=0,readDelay=0;
static struct {int size=0;}commandQueue;
static POINT cursor{160,120};static RECT client{0,0,512,384};static std::function<void(uint32_t)>onRead;static std::function<void()>onSnapshot;
static std::map<uint32_t,uint8_t>memory;
template<class T>static void put(uint32_t at,T value){auto*p=reinterpret_cast<const uint8_t*>(&value);for(size_t n=0;n<sizeof value;++n)memory[at+uint32_t(n)]=p[n];}
static struct Reader {bool readFresh(uint32_t at,void*out,size_t n){++reads;fakeNow+=readDelay;if(onRead)onRead(at);auto*p=static_cast<uint8_t*>(out);for(size_t i=0;i<n;++i){auto found=memory.find(at+uint32_t(i));if(found==memory.end())return false;p[i]=found->second;}return true;}}externalReader;
namespace external {namespace display {struct State {bool visible()const{return ::visible;}};template<class Read>State inspect(Read){return {};}}}
static std::map<int,std::vector<std::wstring>>listItems;static std::map<int,int>listSelections;static std::map<int,std::wstring>captions;
static std::vector<std::wstring>notices;static bool dropped=false;
static DWORD testTick(){return fakeNow;}static BOOL testIsWindow(HWND){return window;}
static BOOL testCursor(POINT*p){*p=cursor;return TRUE;}static BOOL testScreen(HWND,POINT*){return TRUE;}
static BOOL testClient(HWND,RECT*p){*p=client;return TRUE;}static HWND testControl(HWND,int id){return HWND(INT_PTR(id));}
static BOOL testEnable(HWND,BOOL){return TRUE;}
static BOOL testCaption(HWND,int id,const wchar_t*text){captions[id]=text;return TRUE;}
static LRESULT testMessage(HWND h,UINT msg,WPARAM wp,LPARAM lp){int id=int(INT_PTR(h));switch(msg){
 case CB_GETDROPPEDSTATE:return dropped;case CB_GETCOUNT:return listItems[id].size();case CB_GETCURSEL:return listSelections.count(id)?listSelections[id]:-1;
 case CB_SETCURSEL:listSelections[id]=int(wp);return wp;case CB_RESETCONTENT:listItems[id].clear();listSelections[id]=-1;return 0;
 case CB_ADDSTRING:listItems[id].push_back(reinterpret_cast<const wchar_t*>(lp));return listItems[id].size()-1;default:return 0;}}
static LRESULT testDlgMessage(HWND,int id,UINT msg,WPARAM wp,LPARAM lp){return testMessage(HWND(INT_PTR(id)),msg,wp,lp);}
#define EnableWindow testEnable
#define GetTickCount testTick
#define IsWindow testIsWindow
#define GetCursorPos testCursor
#define ScreenToClient testScreen
#define GetClientRect testClient
#define GetDlgItem testControl
#define SetDlgItemTextW testCaption
#define SendMessageW testMessage
#define SendDlgItemMessageW testDlgMessage
static bool foregroundGame(){return focus;}static bool commandFresh(DWORD){return shared&&incoming.status==1&&incoming.commandStatus==1&&epoch!=0;}
static uint32_t commandMatch(){return epoch;}static bool formationInputOwns(){return formed;}
static void readSnapshot(){if(onSnapshot)onSnapshot();}static void wakeGame(){++wakes;}
static void notify(const std::wstring&s){notices.push_back(s);}static std::wstring wide(const char*s){return std::wstring(s,s+strlen(s));}
struct ControlUpdate {ControlUpdate(){}~ControlUpdate(){}};
#include "../source/placement_event_host.h"
static hotkeyroute::Pending hotkeyPending;
static std::map<std::wstring,int>ini;static int value(const wchar_t*s,const wchar_t*k,int fallback){auto i=ini.find(std::wstring(s)+L"/"+k);return i==ini.end()?fallback:i->second;}
static void write(const wchar_t*s,const wchar_t*k,int v){ini[std::wstring(s)+L"/"+k]=v;}
static bool save(){++saved;return true;}static void fillTech(bool){++filled;}static void registerKeys(){++registered;}static void changedSetting(){++settingsChanged;}static void syncAdditional(){++synced;}
static int sentCommand=0,sentArgument=0,sentImplementation=-1;
static void send(int command,int arg=0,int arg2=0);
// Runner extracts these real load/save/UI branches and hotkey dispatch from
// production. No copied version of their routing is allowed in the fixture.
#include HOST_BUILDING_ROUTES
static void check(bool ok){assert(ok);++checks;}
static nativeplacement::Offer offer(int tab=0){nativeplacement::Offer o{};o.match=epoch;o.tick=fakeNow;o.frame=5000;o.slot=tab;o.tab=tab;o.house=0x30000;o.houseID=11;o.factory=0x40000+tab*0x1000;o.factoryID=12+tab;o.product=0x60000+tab*0x1000;o.productID=13+tab;o.type=0x50000+tab*0x1000;o.typeID=14+tab;o.typeIndex=tab;o.typeHash=typeListHash(incoming.types,incoming.typeCount);return o;}
static void setup(bool choose=true){
 storage={};shared=&storage;incoming={};snap={};extra={};extra.placementImplementation=2;techImplementation=3;
 ready=enabled=buildAnywhereMode=focus=visible=window=true;formed=hasSelection=techMode=false;pid=123;epoch=7;ack=0;fakeNow=lastData=1000;sentAt=0;wakes=reads=0;readDelay=0;
 commandQueue.size=0;cursor={160,120};client={0,0,512,384};onRead={};onSnapshot={};memory.clear();notices.clear();listItems.clear();listSelections.clear();captions.clear();dropped=false;hotkeyPending.clear();resetPlacementEvent();
 incoming.status=snap.status=incoming.commandStatus=1;snap.renderStatus=1;incoming.placementStatus=2;incoming.frame=5000;incoming.match=epoch;incoming.typeCount=2;
 strcpy(incoming.types[0].id,"POWER");strcpy(incoming.types[1].id,"TURRET");incoming.types[0].kind=incoming.types[1].kind=3;incoming.types[1].index=1;
 incoming.placementOfferCount=2;incoming.placementOffers[0]=offer();incoming.placementOffers[1]=offer(1);
 put(0x87F7E8+0x539C,int32_t(0));put(0x887324,uint32_t(0x20000));put(0x200B0,int32_t(10));put(0x200B4,int32_t(20));put(0x8A00A4,int32_t(1024));put(0x8A00A8,int32_t(768));put(0xA83D4C,uint32_t(0x30000));put(0xA8ED84,int32_t(5000));
 updatePlacementEventUI();if(choose){listSelections[9251]=1;selectPlacementEvent();}
}
int main(){

 setup(false);check(listItems[9251].size()==2&&placementSelection.current()&&placementSelection.current()->tab==0);submitPlacementEvent();check(wakes==1&&shared->command==PlaceCompletedBuilding&&shared->arg2==1);
 check(shared->arg==0&&shared->requestTypeHash==0&&shared->placementRequest.offer.product==0&&shared->requestTick==1000&&shared->requestMatch==7);
 setup();put(0x87F7E8+0x539C,int32_t(1));updatePlacementEventUI();check(placementSelection.current()&&placementSelection.current()->tab==1);
 setup();incoming.placementOfferCount=0;submitPlacementEvent();check(wakes==1); // Runtime captures live completed factory; host list cannot authorize it.
 for(int reason=0;reason<16;++reason){setup();switch(reason){
  case 0:shared->request=1;break;case 1:shared->selectRequest=1;break;case 2:shared->pointer.request=1;break;case 3:commandQueue.size=1;break;case 4:focus=false;break;case 5:visible=false;break;case 6:incoming.chat=1;break;case 7:formed=true;break;case 8:shared->viewZoomBusy=1;break;case 9:snap.renderStatus=0;break;case 10:extra.placementImplementation=0;break;case 11:buildAnywhereMode=false;break;case 12:enabled=false;break;case 13:incoming.placementStatus=-2;break;case 14:window=false;break;case 15:shared=nullptr;break;}
  submitPlacementEvent();check(wakes==0&&storage.request==(reason==0?1:0));}
 setup();lastData=1;fakeNow=2000;updatePlacementEventUI();check(placementSelection.count==0);
 setup();incoming.commandStatus=2;updatePlacementEventUI();check(placementSelection.count==0);
 setup();hotkeyroute::Group group;group.actions={placementeventhost::Hotkey,1001};group.bindings={uint16_t(extra.placementEventKey),uint16_t('Q')};check(dispatchHotkeyGroup(group));check(wakes==1&&hotkeyPending.count==1&&hotkeyPending.items[0].id==1001);
 setup();commandQueue.size=1;group.actions={placementeventhost::Hotkey};group.bindings={uint16_t(extra.placementEventKey)};check(dispatchHotkeyGroup(group));check(wakes==0&&hotkeyPending.count==0);commandQueue.size=0;check(wakes==0); // no later cursor action
 setup();techMode=true;listSelections[896]=3;fixtureTechRoute(896,CBN_SELCHANGE);check(techImplementation==3&&sentCommand==OpenTech&&sentArgument==1&&sentImplementation==3);check(captions[420].find(L"高危")!=std::wstring::npos);check(captions[476].find(L"高危")==std::wstring::npos);
 for(int command:{Produce,CancelTechProduction}){techImplementation=3;snap.techOpen=0;sentCommand=0;send(command,1);check(sentCommand==0);techImplementation=0;snap.techOpen=4;send(command,1);check(sentCommand==0);snap.techOpen=0;send(command,1);check(sentCommand==command);}
 setup();extra.placementImplementation=0;listSelections[9096]=2;check(fixturePlacementRoute(9096,CBN_SELCHANGE));check(extra.placementImplementation==2&&registered>0&&synced>0&&placementSelection.count==2);
 for(int placement=0;placement<3;++placement)for(int tech=0;tech<4;++tech){extra.placementImplementation=placement;techImplementation=tech;extra.placementEventKey='J'|(2<<8);fixtureSave();extra.placementImplementation=0;techImplementation=0;extra.placementEventKey=0;fixtureLoad();check(extra.placementImplementation==placement&&techImplementation==tech&&extra.placementEventKey==('J'|(2<<8)));}
 ini[L"Experimental/PlacementImplementation"]=99;ini[L"Tech/Implementation"]=-1;fixtureLoad();check(extra.placementImplementation==0&&techImplementation==0);
 settingsdeps::State state;check(settingsdeps::decision(9096,state).enabled());check(!settingsdeps::decision(9251,state).enabled());state.set(settingsdeps::Need::EventPlacement,true);check(settingsdeps::decision(9251,state).enabled()&&settingsdeps::decision(9252,state).enabled());
 check(!settingsdeps::decision(423,state).enabled()&&!settingsdeps::decision(814,state).enabled());state.set(settingsdeps::Need::ToolTechnologyOrders,true);check(settingsdeps::decision(423,state).enabled()&&settingsdeps::decision(814,state).enabled());
 for(int id:{476,807,9096,9251,9252,420,896})check(suite::categories::row(4,&id,1)==suite::categories::Rules);
 for(int id:{455,456,899})check(suite::categories::row(4,&id,1)==suite::categories::Automatic);
 check(std::wstring(buildingmodes::placementCaption(0)).find(L"高危")!=std::wstring::npos&&std::wstring(buildingmodes::technologyCaption(2)).find(L"高危")!=std::wstring::npos);
 extra={};extra.placementImplementation=2;check(!runtimeFeatureDemand(0,extra,false,false));check(runtimeFeatureDemand(BuildAnywhere,extra,false,false));check(runtimeFeatureDemand(0,extra,true,false));
 printf("{\"checks\":%d,\"passed\":true,\"scope\":\"actual Host submission/UI/config/hotkey routes with simulated Win32 and read-only game memory\"}\n",checks);
}
