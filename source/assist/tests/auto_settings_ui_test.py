"""Execute the shipped settings change handler with portable Win32 controls."""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[2]
prefix=r'''
#include "additional_settings.h"
#include "view_zoom.h"
#include "compute_policy.h"
#include "ore_type_config.h"
#include "feature_type_config.h"
#include "protocol.h"
#include <cwchar>
#include <cassert>
#include <algorithm>
#include <map>
#include <vector>
#include <string>
#include <unordered_map>
using namespace assist;
static bool repairUnrepairable=false;static oreconfig::Config oreTypes;static std::wstring oreMiners,oreRefineries,editText;
static featuretypes::Config featureTypes;static std::wstring featureTypeText[featuretypes::Count];static int featureTypeIndex=0;static unsigned featureRefreshes=0;
static void featureTypeFields(){++featureRefreshes;}
static void GetWindowTextW(int,wchar_t* p,int n){wcsncpy(p,editText.c_str(),n-1);p[n-1]=0;}
using UINT_PTR=unsigned;using HWND=int;using WORD=unsigned short;using BOOL=int;
constexpr int FALSE=0,EN_CHANGE=1,CBN_SELCHANGE=2,BN_CLICKED=3,HKM_GETHOTKEY=4,HKM_SETHOTKEY=5,CB_GETCURSEL=6,CB_SETCURSEL=7,BM_GETCHECK=8,BM_SETCHECK=9,BST_CHECKED=1,BST_UNCHECKED=0;
static bool ready=true,updatingControls=false,enabled=true,autoSaveOK=true;
struct ControlUpdate{bool old=updatingControls;ControlUpdate(){updatingControls=true;}~ControlUpdate(){updatingControls=old;}};
static bool hookAdvanced=false,hookSmartUnload=false;static HWND hookChecks[4]={800,801,802,803};
static uint32_t computeRequested=compute::Default;static bool minimapEnabled=false,minimapSecondary=false;static int minimapMode=0,minimapScale=10,advancedRangeCells=2,minimapPosition=0,minimapX=20,minimapY=60,minimapAlignX=0,minimapAlignY=0;
static int minimapSecondaryX=10000,minimapSecondaryY=10000,minimapSecondaryWidth=320;
static bool renderUpgradeEnabled=false,unifiedOverlayEnabled=false,renderMaterialRefresh=true,renderPhysicalOutput=false;
static int renderColorPrecision=1,renderSceneDetail=1,renderUpgradeLayer=0,renderPaletteRange=1,renderPixelScale=3;
constexpr unsigned HOTKEYF_CONTROL=2,HOTKEYF_ALT=4;
constexpr WORD MAKEWORD(unsigned a,unsigned b){return WORD((a&255)|(b<<8));}
static WORD socialKeys[9]{};static bool autoRevealNewMatch=false;static unsigned autoRevealMatch=0,autoRevealDone=0,autoRevealRetry=0;
static WORD idleAircraftKey=MAKEWORD('P',6);
namespace minimap {static void hide(){}static void hidePrimary(){}}static unsigned additionalSyncs=0;static void syncAdditional(){++additionalSyncs;}
static int labelScalePercent=100,constructionScalePercent=100;
static int ui=1,fps=60,topMargin=20,repairMs=2000,doubleMs=350,toastMs=6000;
static bool infoAdaptive=false;static int infoMonitorHz=165;static unsigned infoSyncs=0;
static int informationFPS(){return infoAdaptive?infoMonitorHz:fps;}
static void syncInformationRate(bool force=false){assert(force);++infoSyncs;}
static unsigned settingsRevision=1,paintRevision=0,saves=0,registrations=0,syncs=0,timerMs=0,timers=0,binds=0;
static struct{int status=1,swCount=1;struct{char id[8]="SW";}supers[1];}snap;
static int swList=410;
static std::string selectedSuperID(){return "SW";}
struct Binding{std::string id;WORD key;};static std::vector<Binding>swBindings;
static WORD binding(const char*){return 0;}
static struct{struct{char id[8]="TANK";}types[1];}savedTypes;
static int selectedTypeIndex(){return 0;}
static WORD typeBinding(const char*){return 0;}
static struct{void clear(){}}typeTap;
static AdditionalSettings extra;
static int advancedScanHz=10,advancedTargetPriority=0,shiftClickMode=0,shiftClickGap=0;static bool shiftClickEnabled=false;
namespace airway{static unsigned resets=0;static void reset(){++resets;}}
static struct{unsigned clears=0;void clear(){++clears;}}pointerMatches;
static struct{void clear(){}}infantryEntryResult;
static void updatePlayerTestUI(bool){}
static void send(int){}
static struct{struct Slot{int command;};Slot slots[8]{};int size=0;void discard(int i){for(int j=i+1;j<size;++j)slots[j-1]=slots[j];--size;}}commandQueue;

static int extraControls[ExtraControlCount];
static WORD hotkeys[12]{},csKey=0,advancedKey=0,unloadKey=0,ejectKey=0,csHoldKey=0,magnetronKey=0;
static std::unordered_map<std::string,cooperative::Quota>targetQuotas;
static std::map<int,int> fields;static std::wstring lastMessage;static std::string selectedID="TANK";
static void KillTimer(int,unsigned){timerMs=0;}
static void SetTimer(int,unsigned,int ms,void*){timerMs=ms;++timers;}
static bool save(){++saves;return autoSaveOK;}
static void notify(const std::wstring&s){lastMessage=s;}
static void setStatus(const std::wstring&s){lastMessage=s;}
static void syncTypeConfig(){++syncs;}
static void registerKeys(){++registrations;}
static bool foregroundGame(){return true;}
static void clockState(bool,int){}
static HWND GetDlgItem(int,int id){return id;}
static int GetDlgItemInt(int,int id,BOOL*ok,int){*ok=fields.count(id);return *ok?fields[id]:0;}
static BOOL SetWindowTextW(int id,const wchar_t*value){fields[id]=int(std::wcstol(value,nullptr,10));return 1;}
static int SendMessageW(int id,int msg,unsigned value,int){if(msg==HKM_SETHOTKEY||msg==CB_SETCURSEL||msg==BM_SETCHECK){fields[id]=value;return 0;}return fields[id];}
static bool keyConflict(WORD next,const std::string&,bool){if(extra.allHotkeys||!next)return false;for(auto k:hotkeys)if(k==next)return true;for(auto k:{csKey,advancedKey,unloadKey,ejectKey,csHoldKey,magnetronKey,idleAircraftKey})if(k==next)return true;return false;}
static std::string quotaID(){return selectedID;}
static void readTargetQuota(){ControlUpdate guard;}
static bool extraNumber(int control,int lo,int hi,int&out){auto i=fields.find(700+control);if(i==fields.end()||i->second<lo||i->second>hi)return false;out=i->second;return true;}
static void bindSuper(){++binds;}
static void bindType(bool){++binds;}
'''
io=(root/'assist/source/extra_settings_io.h').read_text()
aircraft_load='extra.aircraftAllocationPolicy='+io.split('extra.aircraftAllocationPolicy=',1)[1].split(';',1)[0]+';'
aircraft_save='write(L"Cooperative",L"AircraftAllocationPolicy",'+io.split('write(L"Cooperative",L"AircraftAllocationPolicy",',1)[1].split(';',1)[0]+';'
load_mouse=' extra.antiTheft='+io.split(' extra.antiTheft=',1)[1].split(' extra.loadMode=',1)[0]
save_mouse=' write(L"Modes",L"AntiTheft"'+io.split(' write(L"Modes",L"AntiTheft"',1)[1].split(' write(L"AutoLoad"',1)[0]
prefix+=r'''
static std::map<std::wstring,int> profile;
static std::map<std::wstring,std::wstring> profile_text;static bool saveWriteOK=true;
static std::wstring path(const wchar_t*s){return s;}
static void GetPrivateProfileStringW(const wchar_t*,const wchar_t*key,const wchar_t* fallback,wchar_t*out,int n,const wchar_t*){auto it=profile_text.find(key);auto text=it==profile_text.end()?std::wstring(fallback):it->second;wcsncpy(out,text.c_str(),n-1);out[n-1]=0;}
static int WritePrivateProfileStringW(const wchar_t*,const wchar_t*key,const wchar_t* text,const wchar_t*){if(text)profile_text[key]=text;else{profile_text.erase(key);profile.erase(key);}return 1;}

static int value(const wchar_t*,const wchar_t* key,int fallback){auto it=profile.find(key);return it==profile.end()?fallback:it->second;}
static std::map<std::wstring,int> burstWrites;
static void write(const wchar_t* section,const wchar_t* key,int v){profile[key]=v;if(std::wstring(section)==L"BurstRefresh")burstWrites[key]=v;}
'''+"\nstatic void loadMouse(){"+load_mouse+"}\nstatic void saveMouse(){"+save_mouse+"}\n"
prefix+="\nstatic void loadAircraftPolicy(){"+aircraft_load+"}\nstatic void saveAircraftPolicy(){"+aircraft_save+"}\n"
feature_load=io.split('static void loadAdditionalSettings(){',1)[1].split(' extra.antiTheft=',1)[0]
feature_save=io.split('static bool saveAdditionalSettings(){',1)[1].split(' write(L"Modes",L"AntiTheft"',1)[0]
prefix+="\nstatic void loadFeatureTypes(){"+feature_load+"}\nstatic void saveFeatureTypes(){"+feature_save+"}\n"
suffix=r''' 
int main(){
 assert(minimapScale==10);
 assert(!extra.viewZoomEnabled&&extra.viewZoomPercent==100);
 assert(extra.dodgeMode==0);
 profile[L"Enabled"]=profile[L"HoldEnabled"]=1;profile[L"HoldKey"]='B';
 extra.burstEnabled=extra.burstHoldEnabled=true;extra.burstKey='B';loadFeatureTypes();
 assert(minimapScale==10); // an INI without MinimapScale takes the new default
 assert(!extra.burstEnabled&&!extra.burstHoldEnabled&&!extra.burstKey);
 saveFeatureTypes();assert(burstWrites.size()==3&&!burstWrites[L"Enabled"]&&!burstWrites[L"HoldEnabled"]&&!burstWrites[L"HoldKey"]);
 for(int id:{882,883,884})assert(!autoSettingChanged(id,BN_CLICKED));
 for(int mode:{0,1}){extra.dodgeMode=mode;saveFeatureTypes();extra.dodgeMode=1-mode;loadFeatureTypes();assert(extra.dodgeMode==mode);}
 extra.dodgeMode=0;
 for(int mode:{1,0}){fields[895]=mode;assert(autoSettingChanged(895,CBN_SELCHANGE));assert(extra.dodgeMode==mode);}
 fields[895]=9;autoSettingChanged(895,CBN_SELCHANGE);assert(extra.dodgeMode==0);
 for(int i=0;i<compute::Count;++i){fields[9000+i]=1;assert(autoSettingChanged(9000+i,CBN_SELCHANGE)&&compute::inside(computeRequested,compute::Feature(i)));fields[9000+i]=0;assert(autoSettingChanged(9000+i,CBN_SELCHANGE)&&!compute::inside(computeRequested,compute::Feature(i)));}
 computeRequested=153;saveFeatureTypes();computeRequested=0;loadFeatureTypes();assert(computeRequested==153);
 for(int legacy:{0,1}){profile.erase(L"MagnetronGlobal");profile.erase(L"MagnetronDrop");profile[L"Magnetron"]=legacy;loadFeatureTypes();assert(compute::inside(computeRequested,compute::MagnetronGlobal)==bool(legacy)&&compute::inside(computeRequested,compute::MagnetronDrop)==bool(legacy));}
 profile[L"MagnetronGlobal"]=0;profile[L"MagnetronDrop"]=1;profile[L"Magnetron"]=0;loadFeatureTypes();assert(!compute::inside(computeRequested,compute::MagnetronGlobal)&&compute::inside(computeRequested,compute::MagnetronDrop));

 minimapEnabled=extra.watchEnabled=false;fields[9162]=fields[9163]=1;assert(autoSettingChanged(9162,BN_CLICKED)&&minimapSecondary);assert(autoSettingChanged(9163,BN_CLICKED)&&extra.watchSecondary);
 assert(!minimapEnabled&&!extra.watchEnabled);saveFeatureTypes();minimapSecondary=extra.watchSecondary=false;loadFeatureTypes();assert(minimapSecondary&&extra.watchSecondary);
 for(int id:{9162,9163}){fields[id]=0;assert(autoSettingChanged(id,BN_CLICKED));}assert(!minimapSecondary&&!extra.watchSecondary);
 assert(extra.watchSecondaryX==10000&&extra.watchSecondaryY==0&&minimapSecondaryX==10000&&minimapSecondaryY==10000&&minimapSecondaryWidth==320);
 for(auto setting:{std::pair<int,int>{9164,0},{9165,9000},{9166,3000},{9167,6500},{9168,1200}}){fields[setting.first]=setting.second;assert(autoSettingChanged(setting.first,EN_CHANGE));}
 assert(extra.watchSecondaryX==0&&extra.watchSecondaryY==9000&&minimapSecondaryX==3000&&minimapSecondaryY==6500&&minimapSecondaryWidth==1200);
 saveFeatureTypes();extra.watchSecondaryX=extra.watchSecondaryY=0;minimapSecondaryX=minimapSecondaryY=minimapSecondaryWidth=0;loadFeatureTypes();
 assert(extra.watchSecondaryX==0&&extra.watchSecondaryY==9000&&minimapSecondaryX==3000&&minimapSecondaryY==6500&&minimapSecondaryWidth==1200);
 auto previousRevision=settingsRevision;auto previousTimers=timers;
 for(int id:{9164,9165,9166,9167})for(int bad:{-1,10001}){fields[id]=bad;autoSettingChanged(id,EN_CHANGE);}
 for(int bad:{159,4097}){fields[9168]=bad;autoSettingChanged(9168,EN_CHANGE);}
 assert(settingsRevision==previousRevision&&timers==previousTimers&&extra.watchSecondaryX==0&&extra.watchSecondaryY==9000&&minimapSecondaryX==3000&&minimapSecondaryY==6500&&minimapSecondaryWidth==1200);
 profile[L"SecondaryX"]=10005;profile[L"SecondaryY"]=-4;profile[L"MinimapSecondaryX"]=-5;profile[L"MinimapSecondaryY"]=10010;profile[L"MinimapSecondaryWidth"]=9999;loadFeatureTypes();
 assert(extra.watchSecondaryX==10000&&extra.watchSecondaryY==0&&minimapSecondaryX==0&&minimapSecondaryY==10000&&minimapSecondaryWidth==4096);
 profile[L"SecondaryX"]=10000;profile[L"SecondaryY"]=0;profile[L"MinimapSecondaryX"]=10000;profile[L"MinimapSecondaryY"]=10000;profile[L"MinimapSecondaryWidth"]=320;loadFeatureTypes();
 fields[830]=1;assert(autoSettingChanged(830,BN_CLICKED)&&minimapEnabled);
 for(int n:{5,9,10,25,50,100,200}){fields[831]=n;assert(autoSettingChanged(831,EN_CHANGE)&&minimapScale==n);}
 for(int bad:{4,201}){fields[831]=bad;autoSettingChanged(831,EN_CHANGE);assert(minimapScale==200);}
 profile[L"MinimapScale"]=1000;loadFeatureTypes();assert(minimapScale==200);
 profile[L"MinimapScale"]=4;loadFeatureTypes();assert(minimapScale==5);
 profile[L"MinimapScale"]=9;loadFeatureTypes();assert(minimapScale==9);
 profile[L"MinimapScale"]=25;loadFeatureTypes();assert(minimapScale==25);
 profile[L"MinimapScale"]=200;loadFeatureTypes();
 for(int mode:{0,1,2,3}){fields[850]=mode;assert(autoSettingChanged(850,CBN_SELCHANGE)&&minimapPosition==mode);}
 for(int n:{0,1}){fields[9065]=n;assert(autoSettingChanged(9065,CBN_SELCHANGE)&&minimapMode==n);}fields[9065]=2;autoSettingChanged(9065,CBN_SELCHANGE);assert(minimapMode==1);
 fields[850]=4;autoSettingChanged(850,CBN_SELCHANGE);assert(minimapPosition==3);
 for(int id:{851,852})for(int n:{0,20,32768}){fields[id]=n;assert(autoSettingChanged(id,EN_CHANGE)&&(id==851?minimapX:minimapY)==n);}
 fields[851]=32769;autoSettingChanged(851,EN_CHANGE);assert(minimapX==32768);
 for(int id:{9100,9101})for(int n:{0,1,2}){fields[id]=n;assert(autoSettingChanged(id,CBN_SELCHANGE)&&(id==9100?minimapAlignX:minimapAlignY)==n);}
 saveFeatureTypes();minimapAlignX=minimapAlignY=0;loadFeatureTypes();assert(minimapAlignX==2&&minimapAlignY==2);
 fields[9100]=3;autoSettingChanged(9100,CBN_SELCHANGE);assert(minimapAlignX==2);
 fields[853]=MAKEWORD('N',6);assert(autoSettingChanged(853,EN_CHANGE)&&idleAircraftKey==MAKEWORD('N',6));
 hotkeys[0]='X';fields[853]='X';autoSettingChanged(853,EN_CHANGE);assert(minimapMode==1&&idleAircraftKey==MAKEWORD('N',6));hotkeys[0]=0;
 saveFeatureTypes();minimapMode=0;idleAircraftKey=0;minimapPosition=minimapX=minimapY=0;loadFeatureTypes();
 assert(idleAircraftKey==MAKEWORD('N',6)&&minimapPosition==3&&minimapX==32768&&minimapY==32768);
 fields[853]=0;autoSettingChanged(853,EN_CHANGE);assert(idleAircraftKey==0);saveFeatureTypes();idleAircraftKey=1;loadFeatureTypes();assert(idleAircraftKey==0);
 for(int id:{816,817}){
  int& scale=id==816?labelScalePercent:constructionScalePercent;
  for(int p:{50,75,100,125,150,200,250}){fields[id]=p;assert(autoSettingChanged(id,EN_CHANGE)&&scale==p&&autoSavePending);}
  for(int bad:{0,49,251,999}){fields[id]=bad;autoSettingChanged(id,EN_CHANGE);assert(scale==250);}
 }

 fields[9090]=1;assert(autoSettingChanged(9090,BN_CLICKED)&&!extra.viewZoomEnabled);
 for(int i=0;i<5;++i){fields[9091]=i;assert(autoSettingChanged(9091,CBN_SELCHANGE)&&extra.viewZoomPercent==100);}
 assert(autoSettingChanged(9092,BN_CLICKED)&&extra.viewZoomPercent==100&&fields[9091]==2);
 fields[9090]=0;assert(autoSettingChanged(9090,BN_CLICKED)&&!extra.viewZoomEnabled);
 loadFeatureTypes();for(int f=0;f<featuretypes::Count;++f)assert(!featureTypes.filters[f].custom);
 for(int f=0;f<featuretypes::Count;++f){featureTypes.filters[f].custom=1;featureTypeText[f]=L"custom_a，CUSTOM_B";}
 saveFeatureTypes();featureTypes={};for(auto&text:featureTypeText)text.clear();loadFeatureTypes();
 for(int f=0;f<featuretypes::Count;++f)assert(featureTypes.filters[f].custom&&featureTypes.filters[f].count==2&&featuretypes::allowed(featureTypes,f,"CUSTOM_A"));
 for(int f=0;f<featuretypes::Count;++f){int toggle=9200+f*2,editor=toggle+1;auto other=featureTypes.filters[(f+1)%featuretypes::Count];
  fields[toggle]=1;autoSettingChanged(toggle,BN_CLICKED);editText=L"UNIT_A,unit_b";autoSettingChanged(editor,EN_CHANGE);
  assert(featureTypes.filters[f].custom&&featureTypes.filters[f].count==2&&featuretypes::allowed(featureTypes,f,"UNIT_B"));
  editText=L"BAD/ID";autoSettingChanged(editor,EN_CHANGE);assert(featureTypes.filters[f].count==2&&featureTypeText[f]==L"UNIT_A,unit_b");
  editText=L"";autoSettingChanged(editor,EN_CHANGE);assert(!featuretypes::allowed(featureTypes,f,"UNIT_A"));assert(!memcmp(&other,&featureTypes.filters[(f+1)%featuretypes::Count],sizeof other));
 }
 // Run the shipped dropdown handler and production profile expressions.
 // Selecting a strategy never enables aircraft control by itself.
 assert(extra.aircraftAllocationPolicy==0&&!extra.air);extra.aircraftAllocationPolicy=1;profile.erase(L"AircraftAllocationPolicy");loadAircraftPolicy();assert(extra.aircraftAllocationPolicy==0);
 for(int policy:{1,2,0}){
  unsigned beforeSync=additionalSyncs,beforeRevision=settingsRevision,beforeClear=pointerMatches.clears;
  fields[9195]=policy;assert(autoSettingChanged(9195,CBN_SELCHANGE)&&extra.aircraftAllocationPolicy==policy&&!extra.air&&autoSavePending);
  assert(additionalSyncs==beforeSync+1&&settingsRevision==beforeRevision+1&&pointerMatches.clears==beforeClear+1);
  saveAircraftPolicy();extra.aircraftAllocationPolicy=(policy+1)%3;loadAircraftPolicy();assert(extra.aircraftAllocationPolicy==policy&&profile[L"AircraftAllocationPolicy"]==policy);
 }
 for(int invalid:{-1,3}){autoSavePending=false;unsigned beforeRevision=settingsRevision,beforeSync=additionalSyncs;fields[9195]=invalid;assert(autoSettingChanged(9195,CBN_SELCHANGE));assert(extra.aircraftAllocationPolicy==0&&!autoSavePending&&settingsRevision==beforeRevision&&additionalSyncs==beforeSync);}
 profile[L"AircraftAllocationPolicy"]=9;loadAircraftPolicy();assert(extra.aircraftAllocationPolicy==2);profile[L"AircraftAllocationPolicy"]=-9;loadAircraftPolicy();assert(extra.aircraftAllocationPolicy==0);saveAircraftPolicy();
 pointerMatches.clears=0;
 autoSavePending=false;timers=saves=syncs=0;timerMs=0;settingsRevision=1;paintRevision=0;

 for(int i=0;i<ExtraControlCount;++i)extraControls[i]=700+i;
 extraControls[AircraftPolicy]=9195;
 assert(!extra.passengerBubbles&&!extra.dangerPassengers&&!extra.magnetronDrop);
 for(auto option:{std::pair<int,bool*>{897,&extra.passengerBubbles},{898,&extra.dangerPassengers},{899,&extra.magnetronDrop}}){
  for(bool on:{false,true}){fields[option.first]=on;autoSettingChanged(option.first,BN_CLICKED);assert(*option.second==on&&autoSavePending);saveFeatureTypes();saveMouse();*option.second=!on;loadMouse();assert(*option.second==on);}
 }
 assert(!extra.allHotkeys);fields[786]=1;autoSettingChanged(786,BN_CLICKED);assert(extra.allHotkeys&&autoSavePending);saveMouse();extra.allHotkeys=false;loadMouse();assert(extra.allHotkeys);
 fields[200]='X';fields[201]='X';autoSettingChanged(200,EN_CHANGE);autoSettingChanged(201,EN_CHANGE);assert(hotkeys[0]=='X'&&hotkeys[1]=='X');
 fields[707]='K';fields[709]='K';autoSettingChanged(707,EN_CHANGE);autoSettingChanged(709,EN_CHANGE);assert(extra.airKey1=='K'&&extra.airKey2=='K');
 fields[786]=0;autoSettingChanged(786,BN_CLICKED);saveMouse();extra.allHotkeys=true;loadMouse();assert(!extra.allHotkeys);hotkeys[0]=hotkeys[1]=0;extra.airKey1=17;extra.airKey2=0;
 assert(!extra.enemyMarkers);fields[785]=1;autoSettingChanged(785,BN_CLICKED);assert(extra.enemyMarkers&&autoSavePending);
 saveMouse();extra.enemyMarkers=false;loadMouse();assert(extra.enemyMarkers);
 fields[785]=0;autoSettingChanged(785,BN_CLICKED);saveMouse();extra.enemyMarkers=true;loadMouse();assert(!extra.enemyMarkers);
 assert(!extra.parasiteMarker&&!extra.crateBuffMarkers);
 for(int id:{892,893}){
  bool& active=id==892?extra.parasiteMarker:extra.crateBuffMarkers;
  bool& other=id==892?extra.crateBuffMarkers:extra.parasiteMarker;
  for(int enabled:{0,1}){
   bool before=other;autoSavePending=false;fields[id]=enabled;
   assert(autoSettingChanged(id,BN_CLICKED)&&active==bool(enabled)&&other==before&&autoSavePending);
   saveMouse();active=!enabled;loadMouse();assert(active==bool(enabled)&&other==before);
  }
 }
 autoSavePending=false;timers=saves=syncs=0;timerMs=0;settingsRevision=1;paintRevision=0;
 fields[310]=144;assert(autoSettingChanged(310,EN_CHANGE));assert(fps==144&&saves==0&&autoSavePending&&timerMs==300&&syncs==1);
 fields[311]=42;autoSettingChanged(311,EN_CHANGE);assert(topMargin==42&&saves==0);flushAutoSettings();assert(saves==1&&!autoSavePending);flushAutoSettings();assert(saves==1);
 fields[310]=0;autoSettingChanged(310,EN_CHANGE);assert(fps==144&&!autoSavePending);
 unsigned rateSyncs=infoSyncs;fields[9160]=BST_CHECKED;
 assert(autoSettingChanged(9160,BN_CLICKED)&&infoAdaptive&&fps==144&&informationFPS()==165&&infoSyncs==rateSyncs+1&&autoSavePending);
 infoMonitorHz=240;assert(informationFPS()==240&&fps==144);
 fields[9160]=BST_UNCHECKED;assert(autoSettingChanged(9160,BN_CLICKED)&&!infoAdaptive&&informationFPS()==144&&infoSyncs==rateSyncs+2);
 ready=false;fields[9160]=BST_CHECKED;assert(!autoSettingChanged(9160,BN_CLICKED)&&!infoAdaptive);ready=true;
 updatingControls=true;assert(!autoSettingChanged(9160,BN_CLICKED)&&!infoAdaptive);updatingControls=false;
 assert(!autoSettingChanged(9160,EN_CHANGE)&&!infoAdaptive);flushAutoSettings();
 fields[701]=400;autoSettingChanged(701,EN_CHANGE);assert(extra.loadRadius==400);fields.erase(701);
 fields[200]='A';autoSettingChanged(200,EN_CHANGE);assert(hotkeys[0]=='A'); // another incomplete numeric field cannot block a key
 fields[201]='A';autoSettingChanged(201,EN_CHANGE);assert(hotkeys[1]==0&&fields[201]==0);
 fields[450]='G';autoSettingChanged(450,EN_CHANGE);assert(advancedKey=='G'&&hookAdvanced&&fields[800]==BST_CHECKED);
 hookAdvanced=false;fields[450]='A';autoSettingChanged(450,EN_CHANGE);assert(advancedKey=='G'&&!hookAdvanced);
 fields[450]=0;autoSettingChanged(450,EN_CHANGE);assert(advancedKey==0&&hookAdvanced);
 fields[703]=0;autoSettingChanged(703,CBN_SELCHANGE);assert(extra.enterKey==0);
 fields[725]=0;autoSettingChanged(725,CBN_SELCHANGE);assert(extra.diskKey==0);
 fields[727]=0;autoSettingChanged(727,CBN_SELCHANGE);assert(extra.sellKey==0);
 fields[728]='H';autoSettingChanged(728,EN_CHANGE);assert(extra.sellKey=='H'&&fields[727]==4);
 fields[728]=0;autoSettingChanged(728,EN_CHANGE);assert(extra.sellKey==0);
 fields[780]=1;autoSettingChanged(780,BN_CLICKED);assert(repairUnrepairable);fields[780]=0;autoSettingChanged(780,BN_CLICKED);assert(!repairUnrepairable);
 fields[781]=1;autoSettingChanged(781,BN_CLICKED);assert(oreTypes.customMiners&&!oreTypes.customRefineries);
 editText=L"HARV,CMIN";autoSettingChanged(783,EN_CHANGE);assert(oreTypes.minerCount==2&&oreMiners==editText);
 editText=L"bad!";autoSettingChanged(783,EN_CHANGE);assert(oreTypes.minerCount==2&&oreMiners==L"HARV,CMIN");
 editText=L"";autoSettingChanged(783,EN_CHANGE);assert(!oreTypes.minerCount&&oreMiners.empty());
 fields[782]=1;autoSettingChanged(782,BN_CLICKED);editText=L"NAREFN";autoSettingChanged(784,EN_CHANGE);assert(oreTypes.refineryCount==1&&oreTypes.customRefineries);
 for(int slot=0;slot<pointerinput::MouseCount;++slot){
  auto before=extra;int value=(slot+2)%5;fields[729+slot]=value;
  assert(autoSettingChanged(729+slot,CBN_SELCHANGE));assert(extra.mouseButtons[slot]==unsigned(value));
  for(int j=0;j<pointerinput::MouseCount;++j)if(j!=slot)assert(extra.mouseButtons[j]==before.mouseButtons[j]);
 }
 fields[729]=-1;auto before=extra;autoSettingChanged(729,CBN_SELCHANGE);assert(extra.mouseButtons[0]==before.mouseButtons[0]);
 fields[729]=4;autoSettingChanged(729,CBN_SELCHANGE);assert(extra.mouseButtons[0]==4);

 fields[447]='C';autoSettingChanged(447,EN_CHANGE);assert(csKey=='C');autoSettingChanged(447,EN_CHANGE);assert(csKey=='C');
 // Refresh mode selection, bounds and persisted defaults/migration.
 profile.clear();loadMouse();assert(extra.scrollRefreshMode==0);
 for(int mode:{0,1,2,3}){fields[886]=mode;assert(autoSettingChanged(886,CBN_SELCHANGE)&&extra.scrollRefreshMode==0);saveMouse();extra.scrollRefreshMode=9;loadMouse();assert(extra.scrollRefreshMode==0);}
 for(int bad:{-1,4}){fields[886]=bad;autoSettingChanged(886,CBN_SELCHANGE);assert(extra.scrollRefreshMode==0);}
#ifndef RA2_UNIFIED
 assert(extra.scrollRefreshHz==120);
 for(int hz:{30,60,120,240,360,500}){fields[887]=hz;assert(autoSettingChanged(887,EN_CHANGE)&&extra.scrollRefreshHz==hz);}
 for(int bad:{0,29,501}){fields[887]=bad;autoSettingChanged(887,EN_CHANGE);assert(extra.scrollRefreshHz==500);}
 saveMouse();extra.scrollRefreshHz=0;loadMouse();assert(extra.scrollRefreshHz==500);
#else
 // The old setting cannot feed the unified UI or be changed by its save path.
 profile[L"ScrollRefreshHz"]=60;assert(!autoSettingChanged(887,EN_CHANGE));saveMouse();assert(profile[L"ScrollRefreshHz"]==60);
#endif
 profile.clear();profile[L"ScrollRefresh"]=1;loadMouse();assert(extra.scrollRefreshMode==0);
 profile[L"RefreshStrategy"]=0;loadMouse();assert(extra.scrollRefreshMode==0);
 profile[L"RefreshStrategy"]=99;profile[L"ScrollRefreshHz"]=900;loadMouse();assert(extra.scrollRefreshMode==0);
#ifndef RA2_UNIFIED
 assert(extra.scrollRefreshHz==500);
#endif
 profile.clear();profile[L"PinKey"]=0x657;loadFeatureTypes();assert(extra.watchKeys[0]==0x657&&extra.watchKeys[1]==0x657&&extra.watchKeys[2]==0x657);
 for(int n=0;n<3;++n){fields[9062+n]=n==2?0:0x8000|'Q';assert(autoSettingChanged(9062+n,EN_CHANGE));}saveFeatureTypes();extra=AdditionalSettings{};loadFeatureTypes();assert(extra.watchKeys[0]==(0x8000|'Q')&&extra.watchKeys[1]==extra.watchKeys[0]&&!extra.watchKeys[2]);
 fields[9057]='Z';fields[9058]='Z';assert(autoSettingChanged(9057,EN_CHANGE)&&autoSettingChanged(9058,EN_CHANGE));saveFeatureTypes();extra=AdditionalSettings{};loadFeatureTypes();assert(extra.formationKeys[4]=='Z'&&extra.formationKeys[5]=='Z'&&extra.formationMouse==2);
 profile.clear();loadFeatureTypes();
 // Experimental reveal opt-ins are exclusive and the render path clears queued old reveals.
 extra.experimentalReveal=true;commandQueue.size=3;commandQueue.slots[0].command=RevealMap;commandQueue.slots[1].command=15;commandQueue.slots[2].command=RevealMap;
 fields[808]=1;autoSettingChanged(808,CBN_SELCHANGE);assert(extra.renderReveal&&!extra.experimentalReveal&&autoSavePending);
 assert(commandQueue.size==1&&commandQueue.slots[0].command==15);
 saveMouse();extra.renderReveal=false;loadMouse();assert(extra.renderReveal&&!extra.experimentalReveal);
 profile[L"RevealMap"]=1;loadMouse();assert(extra.renderReveal&&!extra.experimentalReveal); // legacy permission loses on load
 fields[808]=4;autoSettingChanged(808,CBN_SELCHANGE);assert(extra.renderReveal==3&&!extra.experimentalReveal);saveFeatureTypes();saveMouse();loadFeatureTypes();loadMouse();assert(extra.mapStrategy==4&&extra.renderReveal==3);
 fields[9096]=1;autoSettingChanged(9096,CBN_SELCHANGE);fields[9097]=1;autoSettingChanged(9097,BN_CLICKED);saveFeatureTypes();saveMouse();extra.placementImplementation=0;extra.techEnhance=false;loadFeatureTypes();loadMouse();assert(extra.placementImplementation==1&&extra.techEnhance);
 fields[808]=0;autoSettingChanged(808,CBN_SELCHANGE);assert(!extra.renderReveal);saveFeatureTypes();saveMouse();loadMouse();assert(!extra.renderReveal);
 fields[803]=2;autoSettingChanged(803,CBN_SELCHANGE);assert(extra.nativeAirWaypoints&&airway::resets==1&&pointerMatches.clears==1);
 fields[803]=0;autoSettingChanged(803,CBN_SELCHANGE);assert(!extra.nativeAirWaypoints&&airway::resets==2&&pointerMatches.clears==2);
 for(int mode:{0,1,2,3,4}){fields[803]=mode;autoSettingChanged(803,CBN_SELCHANGE);assert(extra.airWaypoints==(mode==1)&&extra.nativeAirWaypoints==(mode>=2)&&extra.nativeAirNetwork==unsigned(mode>=3?mode-2:0));saveMouse();extra.airWaypoints=extra.nativeAirWaypoints=false;extra.nativeAirNetwork=0;loadMouse();assert(extra.airWaypoints==(mode==1)&&extra.nativeAirWaypoints==(mode>=2)&&extra.nativeAirNetwork==unsigned(mode>=3?mode-2:0));}
 for(unsigned key:{0u,unsigned(MAKEWORD('W',2)),0u}){fields[894]=key;autoSettingChanged(894,EN_CHANGE);assert(extra.airPathKey==key);saveMouse();extra.airPathKey=123;loadMouse();assert(extra.airPathKey==key);}
 fields[787]=60;autoSettingChanged(787,EN_CHANGE);assert(advancedScanHz==60);fields[787]=61;autoSettingChanged(787,EN_CHANGE);assert(advancedScanHz==60);
 assert(!extra.experimentalReveal);profile[L"RevealMap"]=1;loadMouse();assert(!extra.experimentalReveal);
 extra.experimentalReveal=false;saveMouse();extra.experimentalReveal=true;loadMouse();assert(!extra.experimentalReveal);
 profile.clear();profile[L"EnhancedButton"]=3;profile[L"AttackMoveButton"]=1;profile[L"SellButton"]=2;loadMouse();
 for(int i=0;i<pointerinput::MouseCount;++i)assert(extra.mouseButtons[i]==unsigned(i<pointerinput::MouseAttack?3:i==pointerinput::MouseAttack?1:2));
 profile[L"AdvancedButton"]=0;profile[L"MagnetronButton"]=4;profile[L"CSButton"]=99;loadMouse();
 assert(extra.mouseButtons[0]==0&&extra.mouseButtons[1]==4&&extra.mouseButtons[2]==6);
 auto saved=extra;saveMouse();extra=AdditionalSettings{};loadMouse();
 for(int i=0;i<pointerinput::MouseCount;++i)assert(extra.mouseButtons[i]==saved.mouseButtons[i]);
 assert(profile[L"EnhancedButton"]==3&&profile[L"SellButton"]==2);
 updatingControls=true;fields[200]='Z';assert(!autoSettingChanged(200,EN_CHANGE));assert(hotkeys[0]=='A');updatingControls=false;
 ready=false;assert(!autoSettingChanged(310,EN_CHANGE));ready=true;
 fields[700]=1;autoSettingChanged(700,CBN_SELCHANGE);assert(extra.loadMode==1);
 fields[796]=2;assert(autoSettingChanged(796,CBN_SELCHANGE)&&shiftClickMode==2);
 fields[802]=1;assert(autoSettingChanged(802,CBN_SELCHANGE)&&advancedTargetPriority==1);
 fields[812]=5;assert(autoSettingChanged(812,CBN_SELCHANGE)&&extra.c4Mouse==6);
 // Transient empty selections must retain runtime settings and never queue a
 // write that would overwrite the preceding valid choice.
 for(const auto& [id,max]:{std::pair<int,int>{700,1},{796,2},{802,1},{812,5}}){
  for(int invalid:{-1,max+1}){
   autoSavePending=false;unsigned beforeRevision=settingsRevision,beforePaint=paintRevision,beforeTimers=timers;
   fields[id]=invalid;assert(autoSettingChanged(id,CBN_SELCHANGE));
   assert(extra.loadMode==1&&shiftClickMode==2&&advancedTargetPriority==1&&extra.c4Mouse==6);
   assert(!autoSavePending&&settingsRevision==beforeRevision&&paintRevision==beforePaint&&timers==beforeTimers);
  }
 }
 fields[704]='G';autoSettingChanged(704,EN_CHANGE);assert(extra.enterKey=='G'&&fields[703]==4);
 fields[707]='K';autoSettingChanged(707,EN_CHANGE);assert(extra.airKey1=='K');fields[709]='K';autoSettingChanged(709,EN_CHANGE);assert(extra.airKey2==0&&fields[708]==0);
 fields[716]=4;fields[717]=5;fields[718]=6;autoSettingChanged(716,EN_CHANGE);assert(targetQuotas.at("TANK").first==4&&fields[715]==1);
 fields.erase(716);fields[715]=0;autoSettingChanged(715,BN_CLICKED);assert(targetQuotas.empty());
 fields[724]='V';autoSettingChanged(724,EN_CHANGE);assert(extra.variantKey=='V');
 autoSettingChanged(441,EN_CHANGE);autoSettingChanged(411,EN_CHANGE);assert(binds==2);
 // Manual control and the separate enemy-sale availability gate stay independent.
 extra.controlAll=extra.sellEnemy=false;fields[860]=BST_CHECKED;
 assert(autoSettingChanged(860,BN_CLICKED)&&extra.controlAll&&!extra.sellEnemy);
 fields[722]=BST_CHECKED;assert(autoSettingChanged(722,BN_CLICKED)&&extra.controlAll&&extra.sellEnemy==playertest::SellAvailable);
 fields[860]=BST_UNCHECKED;assert(autoSettingChanged(860,BN_CLICKED)&&!extra.controlAll&&extra.sellEnemy==playertest::SellAvailable);
 extra.switchPlayerEnabled=true;fields[861]=BST_CHECKED;
 assert(autoSettingChanged(861,BN_CLICKED)&&!extra.switchPlayerEnabled&&extra.sellEnemy==playertest::SellAvailable);
 assert(fields[861]==BST_UNCHECKED);
 fields[722]=BST_UNCHECKED;assert(autoSettingChanged(722,BN_CLICKED)&&!extra.sellEnemy);
 autoSaveOK=false;flushAutoSettings();assert(lastMessage.find(L"自动保存失败")!=std::wstring::npos);
}
'''
with tempfile.TemporaryDirectory() as temp:
 cpp=Path(temp)/'auto.cpp';binary=Path(temp)/'auto';cpp.write_text(prefix+'\n#include "auto_settings_ui.h"\n'+suffix)
 for defines in ([],['-DRA2_UNIFIED']):
  subprocess.run(['g++','-std=c++17','-O0',*defines,'-I'+str(root/'assist/source'),str(cpp),'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True)
main=(root/'assist/source/main.cpp').read_text()
extra=(root/'assist/source/extra_controls.h').read_text()
assert 'statsVisible()' not in main
assert 'snap.renderStatus!=1' in main.split('static void paint(){')[1].split('DWORD now=')[0]
close=main.split('case WM_CLOSE:',1)[1].split('case WM_DESTROY:',1)[0]
assert close.index('flushAutoSettings();')<close.index('DestroyWindow(w)')
assert 'L"BUTTON",L"保存' not in main+extra
assert 'L"BUTTON",L"绑定' not in main
for module in ('assist','unit_monitor'):
 src=(root/module/'source/main.cpp').read_text()
 assert 'external::ReadSession' in src and 'external::ReadScope' not in src
print('PASS: information adaptive/manual notifications retain the manual rate; refresh mode bounds, persistence and legacy migration; shipped settings handler applies/flushes valid edits, preserves conflicting keys, ignores programmatic updates, saves quotas/custom keys; independent overlays and common read leases')

# Hook permissions are linked only after explicit, valid user activation.
assert (main+(root/'assist/source/hotkey_host.h').read_text()).count('if(csMode){hookAutoCS=true;SendMessageW(hookChecks[3],BM_SETCHECK,BST_CHECKED,0);}')>=2
assert 'hookAutoCS=value(L"Hooks",L"AutoCS",0)' in main
assert 'value(L"NativeOrders",L"EnableInputHooks",0)' in main
import re
assert not re.search(r'shared->(?:ack|result)\s*=(?!=)',main)
cursor=(root/'cursor/source/main.cpp').read_text()
assert 'return external::discovery::isGameName(s);' in cursor
assert 'external::discovery::resolveYrppWindow(reader,pid,nullptr,&foundThread)' in cursor
assert 'static BOOL CALLBACK findWindow(' not in cursor
print('PASS: explicit feature/permission linkage, invalid-key isolation, shared cursor discovery and host ACK single-writer contract')

assert 'if(snap.renderStatus!=1){overlayRecovery.suspend();hideWorldOverlay();hideAircraftButton();minimap::hide();watchwindows::hide();' in main
assert 'else if(!foregroundGame()||snap.chat){overlayRecovery.suspend();hideWorldOverlay();hideAircraftButton();minimap::hidePrimary();watchwindows::hidePrimary();' in main
assert 'snap.renderStatus=externalValid&&d.renderStatus==1?1:0;' in main
monitor=(root/'unit_monitor/source/main.cpp').read_text() if (root/'unit_monitor/source/main.cpp').exists() else (root.parent/'unit_monitor/source/main.cpp').read_text()
assert 'external::display::inspect' in monitor and 'battlefield&&foregroundGame()' in monitor
print('PASS shared menu visibility is consumed by both overlays; control tick hides even when render clock stops')

assert 'tabEnabled&&enabled&&gameActive&&!demo' in monitor

assert 'if(c==RevealMap&&!extra.experimentalReveal)return false;' in main
assert 'if(command==RevealMap&&!extra.experimentalReveal)' in main
assert 'if(autoRevealNewMatch&&extra.experimentalReveal&&' in main
assert 'hotkeys[i]=i==8?0:' in main
assert 'case 116:toggleOverlay();' in monitor
assert 'if(a==1){toggleOverlay();}' in monitor
assert 'tabui::displayRequested(hidden,demo||!tabEnabled,manualVisible,tabs.visible())' in monitor
assert '常驻显示（不依赖 Tab）' in monitor
print('PASS experimental reveal default/roundtrip and all host gates; independent manual overlay controls retain battlefield gating')
print('PASS parasite/crate badges default off; independent controls 892/893 apply, autosave and persist both states')

assert 'value(L"Dodge",L"Logic",0)' in (root/'assist/source/extra_settings_io.h').read_text()
assert 'write(L"Dodge",L"Logic",extra.dodgeMode)' in (root/'assist/source/extra_settings_io.h').read_text()
print("PASS reverse-dodge default, both live choices, invalid selection and persistence key")

tactical=(root/'assist/source/tactical_controls.h').read_text()
assert 'extraControls[AircraftPolicy]=combo(9195)' in tactical
assert 'L"按数量配额（原模式 1 / 2）",L"5格战车 · 预计伤害 120%",L"单目标 120% · 派出后取消选中"' in tactical
assert 'AircraftAllocationPolicy=0' in (root/'defaults/Assist.ini').read_text()
assert 'AircraftAllocationPolicy=0' in (root/'shared/source/initial_profiles.h').read_text()
print('PASS aircraft allocation strategies: legacy default, all three choices, immediate sync, autosave, invalid-selection isolation and production INI roundtrips')

print('PASS passenger inspection, persistent dangerous cargo and independent magnetron drop: UI toggle, autosave, persisted off/on roundtrips')

