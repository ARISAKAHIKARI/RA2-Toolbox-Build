#ifdef NDEBUG
#undef NDEBUG
#endif
#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <array>
#include <string>
#include <map>
#include <cstdio>
#include <cassert>
#include "../source/additional_settings.h"
#include "../source/runtime_feature_demand.h"
#include "../../suite/source/settings_categories.h"
#include "../../external_reader/source/projection.h"
using namespace assist;
static AdditionalSettings extra;
struct PanelOptions {bool detached=false,collapsed=false;int x=20,y=150,scale=100,rows=8;} superweaponOptions;
static std::map<std::pair<std::wstring,std::wstring>,int> ini;
static int value(const wchar_t*s,const wchar_t*k,int fallback){auto i=ini.find({s,k});return i==ini.end()?fallback:i->second;}
static bool write(const wchar_t*s,const wchar_t*k,int v){ini[{s,k}]=v;return true;}
static unsigned changed=0,synced=0;
static void changedSetting(){++changed;}static void syncAdditional(){++synced;}
static void registerKeys(){}
struct MatchState {int clears=0;void clear(){++clears;}} pointerMatches;
static HWND ui=nullptr;
static std::map<int,LRESULT> controlValues;
static HWND fixtureItem(HWND,int id){return reinterpret_cast<HWND>(intptr_t(id));}
static LRESULT fixtureSend(HWND control,UINT,WPARAM,LPARAM){return controlValues[int(intptr_t(control))];}
#define GetDlgItem fixtureItem
#define SendMessageW fixtureSend
static uint32_t clockNow=1000,pid=20,matchId=30;
static bool enabled=true,foreground=true,fresh=true;
static DWORD fixtureClock(){return clockNow;}
static std::array<uint8_t,256> physical{},recorded{};
static short fixtureKey(int key){return physical[key]?short(0x8000):0;}
#define GetTickCount fixtureClock
#define GetAsyncKeyState fixtureKey
static uint32_t commandMatch(){return matchId;}static bool foregroundGame(){return foreground;}static bool commandFresh(uint32_t){return fresh;}
struct State{bool chat=false;int cameraX=0,cameraY=0,width=800,height=600;}snap;
struct Wire {long moGesturePolicySeq=0,moOrdersPreviewSeq=0;moorders::GesturePolicy moGestures;uint32_t moFeatures=0;struct{int enabled=0;}formationPolicy;moorders::Preview moOrdersPreview,formationPreview;} wire;
static Wire*shared=&wire;static int hook=0;
namespace gameinput {static void keySnapshot(int,uint8_t*keys){std::copy(recorded.begin(),recorded.end(),keys);}}
namespace modernui {static std::wstring hotkeyName(WORD key){return std::to_wstring(key);}}
#include "../source/middle_input_host.h"
#include GESTURE_HOST_PRODUCTION
static unsigned checks=0;
static void check(bool ok){++checks;if(!ok){std::printf("FAIL check %u\n",checks);std::fflush(stdout);std::abort();}}
int main(){
 check(extra.moFeatures==0&&!extra.superweaponPanel&&!runtimeFeatureDemand(0,extra,false,false));
 loadNewSettings();check(extra.moFeatures==0&&extra.moGestures.keys[0]==17&&extra.moGestures.buttons[3]==1);
 for(int i=0;i<4;++i){controlValues[9440+2*i]='G'+i;check(changeNewSettings(9440+2*i,EN_CHANGE));controlValues[9441+2*i]=5;check(changeNewSettings(9441+2*i,CBN_SELCHANGE));check(extra.moGestures.keys[i]==uint32_t('G'+i)&&extra.moGestures.buttons[i]==6);}
 for(int id=9400;id<=9402;++id){controlValues[id]=BST_CHECKED;check(changeNewSettings(id,BN_CLICKED));check(runtimeFeatureDemand(0,extra,false,false));}
 check(extra.moFeatures==7&&pointerMatches.clears==11&&synced==11);
 const auto oldSync=synced,oldChange=changed;check(changeNewSettings(9403,BN_CLICKED)&&changeNewSettings(9404,CBN_SELCHANGE));check(synced==oldSync+2&&changed==oldChange+2&&extra.moFeatures==7);
 for(int id=9450;id<=9455;++id){controlValues[id]='R'+id-9450;check(changeNewSettings(id,EN_CHANGE));}check(synced==oldSync+8);
 controlValues[9405]=BST_CHECKED;check(changeNewSettings(9405,BN_CLICKED)&&extra.superweaponPanel);
 superweaponOptions={true,true,123,456,150,12};saveNewSettings();auto saved=extra;
 extra=AdditionalSettings{};superweaponOptions={};loadNewSettings();check(extra.moFeatures==7&&extra.superweaponPanel&&moorders::sameGestures(extra.moGestures,saved.moGestures));
 check(extra.viewZoomKeys[0]==saved.viewZoomKeys[0]&&extra.sidebarKey==saved.sidebarKey&&extra.pauseKeys[1]==saved.pauseKeys[1]);check(superweaponOptions.detached&&superweaponOptions.collapsed&&superweaponOptions.x==123&&superweaponOptions.rows==12);
 ini[{L"MOControls",L"Features"}]=31;ini[{L"Mouse",L"FuzzyOrderMouseButton"}]=5;ini[{L"Display",L"SuperweaponScale"}]=500;loadNewSettings();check(extra.moFeatures==23&&extra.moGestures.buttons[0]==0&&superweaponOptions.scale==200);
 publishNewSettings();check(wire.moFeatures==23&&wire.moGesturePolicySeq==2&&moorders::sameGestures(wire.moGestures,extra.moGestures));
 extra=AdditionalSettings{};extra.moFeatures=1;publishNewSettings();physical[4]=physical[17]=1;check(runtimeMiddleOwnsNow());
 foreground=false;++clockNow;check(!runtimeMiddleOwnsNow());foreground=true;fresh=false;++clockNow;check(!runtimeMiddleOwnsNow());fresh=true;snap.chat=true;++clockNow;check(!runtimeMiddleOwnsNow());snap.chat=false;
 physical={};++clockNow;wire.moOrdersPreview.active=1;wire.moOrdersPreview.match=matchId;check(runtimeMiddleOwnsNow());wire.moOrdersPreview.match=matchId-1;check(!runtimeMiddleOwnsNow());wire.moOrdersPreview.active=0;
 // Captured down edges are reserved even after release, before old pointer features.
 uint8_t edge[256]{};edge[17]=1;check(!oldPointerAllowed(1,edge));check(oldPointerAllowed(2,edge));
 wire.moGestures.keys[0]='G'|0xb000;physical['G']=1;++clockNow;check(!runtimeMiddleOwnsNow());physical['G']=0;++clockNow;runtimeMiddleOwnsNow();foreground=false;++clockNow;runtimeMiddleOwnsNow();foreground=true;physical['G']=physical[4]=1;++clockNow;check(!runtimeMiddleOwnsNow());
 // Policy seqlock fallback is seeded from this session's configuration.
 ++matchId;wire.moGesturePolicySeq|=1;extra.moGestures.keys[0]='H';check(runtimeGesturePolicy().keys[0]=='H');wire.moGesturePolicySeq++;
 Drawing a,b;check(sameDrawing(a,b));a.active=b.active=true;a.preview.count=b.preview.count=1;a.preview.nodeCount=b.preview.nodeCount=1;check(sameDrawing(a,b));b.preview.tick=99;check(sameDrawing(a,b));b.preview.lines[0].to.x=1;check(!sameDrawing(a,b));b=a;b.projection.cameraX=1;check(!sameDrawing(a,b));b=a;b.hint=L"new key";check(!sameDrawing(a,b));b=a;b.preview.nodes[0].z=1;check(!sameDrawing(a,b));
 {int id=9405;check(suite::categories::row(4,&id,1)==suite::categories::Video);}
 for(int id:{9404,9407,9450,9451,9452,9453,9403,9406,9454,9455,9462,9463,9464})check(suite::categories::row(4,&id,1)==suite::categories::Experimental);
 for(int id:{9400,9401,9402,9440,9446,9460})check(suite::categories::row(1,&id,1)==suite::categories::Control);
 std::printf("PASS %u actual Host config/dispatch/IPC/arbitration/focus/preview/category checks\n",checks);
}
