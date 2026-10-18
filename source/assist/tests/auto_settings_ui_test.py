"""Execute the shipped settings change handler with portable Win32 controls."""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[2]
prefix=r'''
#include "additional_settings.h"
#include "ore_type_config.h"
#include "feature_type_config.h"
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
static bool hookAdvanced=false;static HWND hookChecks[4]={800,801,802,803};
static int ui=1,fps=60,topMargin=20,repairMs=2000,doubleMs=350,toastMs=6000;
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
static constexpr int RevealMap=9;
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
static int SendMessageW(int id,int msg,unsigned value,int){if(msg==HKM_SETHOTKEY||msg==CB_SETCURSEL||msg==BM_SETCHECK){fields[id]=value;return 0;}return fields[id];}
static bool keyConflict(WORD next,const std::string&,bool){if(extra.allHotkeys||!next)return false;for(auto k:hotkeys)if(k==next)return true;for(auto k:{csKey,advancedKey,unloadKey,ejectKey,csHoldKey,magnetronKey})if(k==next)return true;return false;}
static std::string quotaID(){return selectedID;}
static void readTargetQuota(){ControlUpdate guard;}
static bool extraNumber(int control,int lo,int hi,int&out){auto i=fields.find(700+control);if(i==fields.end()||i->second<lo||i->second>hi)return false;out=i->second;return true;}
static void bindSuper(){++binds;}
static void bindType(bool){++binds;}
'''
io=(root/'assist/source/extra_settings_io.h').read_text()
load_mouse=' extra.antiTheft='+io.split(' extra.antiTheft=',1)[1].split(' extra.loadMode=',1)[0]
save_mouse=' write(L"Modes",L"AntiTheft"'+io.split(' write(L"Modes",L"AntiTheft"',1)[1].split(' write(L"AutoLoad"',1)[0]
prefix+=r'''
static std::map<std::wstring,int> profile;
static std::map<std::wstring,std::wstring> profile_text;static bool saveWriteOK=true;
static std::wstring path(const wchar_t*s){return s;}
static void GetPrivateProfileStringW(const wchar_t*,const wchar_t*key,const wchar_t* fallback,wchar_t*out,int n,const wchar_t*){auto it=profile_text.find(key);auto text=it==profile_text.end()?std::wstring(fallback):it->second;wcsncpy(out,text.c_str(),n-1);out[n-1]=0;}
static int WritePrivateProfileStringW(const wchar_t*,const wchar_t*key,const wchar_t* text,const wchar_t*){profile_text[key]=text;return 1;}

static int value(const wchar_t*,const wchar_t* key,int fallback){auto it=profile.find(key);return it==profile.end()?fallback:it->second;}
static void write(const wchar_t*,const wchar_t* key,int v){profile[key]=v;}
'''+"\nstatic void loadMouse(){"+load_mouse+"}\nstatic void saveMouse(){"+save_mouse+"}\n"
feature_load=io.split('static void loadAdditionalSettings(){',1)[1].split(' extra.antiTheft=',1)[0]
feature_save=io.split('static bool saveAdditionalSettings(){',1)[1].split(' write(L"Modes",L"AntiTheft"',1)[0]
prefix+="\nstatic void loadFeatureTypes(){"+feature_load+"}\nstatic void saveFeatureTypes(){"+feature_save+"}\n"
suffix=r''' 
int main(){
 loadFeatureTypes();for(int f=0;f<featuretypes::Count;++f)assert(!featureTypes.filters[f].custom);
 for(int f=0;f<featuretypes::Count;++f){featureTypes.filters[f].custom=1;featureTypeText[f]=L"custom_a，CUSTOM_B";}
 saveFeatureTypes();featureTypes={};for(auto&text:featureTypeText)text.clear();loadFeatureTypes();
 for(int f=0;f<featuretypes::Count;++f)assert(featureTypes.filters[f].custom&&featureTypes.filters[f].count==2&&featuretypes::allowed(featureTypes,f,"CUSTOM_A"));
 for(int f=0;f<featuretypes::Count;++f){fields[790]=f;assert(autoSettingChanged(790,CBN_SELCHANGE)&&featureTypeIndex==f&&featureRefreshes==unsigned(f+1));
  fields[791]=1;autoSettingChanged(791,BN_CLICKED);editText=L"UNIT_A,unit_b";autoSettingChanged(792,EN_CHANGE);
  assert(featureTypes.filters[f].custom&&featureTypes.filters[f].count==2&&featuretypes::allowed(featureTypes,f,"UNIT_B"));
  editText=L"BAD/ID";autoSettingChanged(792,EN_CHANGE);assert(featureTypes.filters[f].count==2&&featureTypeText[f]==L"UNIT_A,unit_b");
  editText=L"";autoSettingChanged(792,EN_CHANGE);assert(!featuretypes::allowed(featureTypes,f,"UNIT_A"));
 }
 autoSavePending=false;timers=saves=syncs=0;timerMs=0;settingsRevision=1;paintRevision=0;

 for(int i=0;i<ExtraControlCount;++i)extraControls[i]=700+i;
 assert(!extra.allHotkeys);fields[786]=1;autoSettingChanged(786,BN_CLICKED);assert(extra.allHotkeys&&autoSavePending);saveMouse();extra.allHotkeys=false;loadMouse();assert(extra.allHotkeys);
 fields[200]='X';fields[201]='X';autoSettingChanged(200,EN_CHANGE);autoSettingChanged(201,EN_CHANGE);assert(hotkeys[0]=='X'&&hotkeys[1]=='X');
 fields[707]='K';fields[709]='K';autoSettingChanged(707,EN_CHANGE);autoSettingChanged(709,EN_CHANGE);assert(extra.airKey1=='K'&&extra.airKey2=='K');
 fields[786]=0;autoSettingChanged(786,BN_CLICKED);saveMouse();extra.allHotkeys=true;loadMouse();assert(!extra.allHotkeys);hotkeys[0]=hotkeys[1]=0;extra.airKey1=17;extra.airKey2=0;
 assert(!extra.enemyMarkers);fields[785]=1;autoSettingChanged(785,BN_CLICKED);assert(extra.enemyMarkers&&autoSavePending);
 saveMouse();extra.enemyMarkers=false;loadMouse();assert(extra.enemyMarkers);
 fields[785]=0;autoSettingChanged(785,BN_CLICKED);saveMouse();extra.enemyMarkers=true;loadMouse();assert(!extra.enemyMarkers);
 autoSavePending=false;timers=saves=syncs=0;timerMs=0;settingsRevision=1;paintRevision=0;
 fields[310]=144;assert(autoSettingChanged(310,EN_CHANGE));assert(fps==144&&saves==0&&autoSavePending&&timerMs==300&&syncs==1);
 fields[311]=42;autoSettingChanged(311,EN_CHANGE);assert(topMargin==42&&saves==0);flushAutoSettings();assert(saves==1&&!autoSavePending);flushAutoSettings();assert(saves==1);
 fields[310]=0;autoSettingChanged(310,EN_CHANGE);assert(fps==144&&!autoSavePending);
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
 // Experimental reveal opt-ins are exclusive and the render path clears queued old reveals.
 extra.experimentalReveal=true;commandQueue.size=3;commandQueue.slots[0].command=RevealMap;commandQueue.slots[1].command=15;commandQueue.slots[2].command=RevealMap;
 fields[808]=1;autoSettingChanged(808,BN_CLICKED);assert(extra.renderReveal&&!extra.experimentalReveal&&autoSavePending);
 assert(commandQueue.size==1&&commandQueue.slots[0].command==15&&fields[739]==BST_UNCHECKED);
 saveMouse();extra.renderReveal=false;loadMouse();assert(extra.renderReveal&&!extra.experimentalReveal);
 profile[L"RevealMap"]=1;loadMouse();assert(extra.renderReveal&&!extra.experimentalReveal); // legacy permission loses on load
 fields[808]=0;autoSettingChanged(808,BN_CLICKED);assert(!extra.renderReveal);saveMouse();loadMouse();assert(!extra.renderReveal);
 fields[806]=1;autoSettingChanged(806,BN_CLICKED);assert(extra.nativeAirWaypoints&&airway::resets==1&&pointerMatches.clears==1);
 fields[806]=0;autoSettingChanged(806,BN_CLICKED);assert(!extra.nativeAirWaypoints&&airway::resets==2&&pointerMatches.clears==2);
 fields[787]=60;autoSettingChanged(787,EN_CHANGE);assert(advancedScanHz==60);fields[787]=61;autoSettingChanged(787,EN_CHANGE);assert(advancedScanHz==60);
 assert(!extra.experimentalReveal);profile[L"RevealMap"]=1;loadMouse();assert(extra.experimentalReveal);
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
 fields[704]='G';autoSettingChanged(704,EN_CHANGE);assert(extra.enterKey=='G'&&fields[703]==4);
 fields[707]='K';autoSettingChanged(707,EN_CHANGE);assert(extra.airKey1=='K');fields[709]='K';autoSettingChanged(709,EN_CHANGE);assert(extra.airKey2==0&&fields[708]==0);
 fields[716]=4;fields[717]=5;fields[718]=6;autoSettingChanged(716,EN_CHANGE);assert(targetQuotas.at("TANK").first==4&&fields[715]==1);
 fields.erase(716);fields[715]=0;autoSettingChanged(715,BN_CLICKED);assert(targetQuotas.empty());
 fields[724]='V';autoSettingChanged(724,EN_CHANGE);assert(extra.variantKey=='V');
 autoSettingChanged(441,EN_CHANGE);autoSettingChanged(411,EN_CHANGE);assert(binds==2);
 autoSaveOK=false;flushAutoSettings();assert(lastMessage.find(L"自动保存失败")!=std::wstring::npos);
}
'''
with tempfile.TemporaryDirectory() as temp:
 cpp=Path(temp)/'auto.cpp';binary=Path(temp)/'auto';cpp.write_text(prefix+'\n#include "auto_settings_ui.h"\n'+suffix)
 subprocess.run(['g++','-std=c++17','-O0','-I'+str(root/'assist/source'),str(cpp),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
main=(root/'assist/source/main.cpp').read_text()
extra=(root/'assist/source/extra_controls.h').read_text()
assert 'statsVisible()' not in main
assert 'snap.renderStatus!=1' in main.split('static void paint(){')[1].split('DWORD now=')[0]
assert 'flushAutoSettings();DestroyWindow(w)' in main
assert 'L"BUTTON",L"保存' not in main+extra
assert 'L"BUTTON",L"绑定' not in main
for module in ('assist','unit_monitor'):
 src=(root/module/'source/main.cpp').read_text()
 assert 'external::ReadSession' in src and 'external::ReadScope' not in src
print('PASS: shipped settings handler applies/flushes valid edits, preserves conflicting keys, ignores programmatic updates, saves quotas/custom keys; independent overlays and common read leases')

# Hook permissions are linked only after explicit, valid user activation.
assert (main+(root/'assist/source/hotkey_host.h').read_text()).count('if(csMode){hookAutoCS=true;SendMessageW(hookChecks[3],BM_SETCHECK,BST_CHECKED,0);}')>=2
assert 'hookAutoCS=value(L"Hooks",L"AutoCS",1)' in main
assert 'value(L"NativeOrders",L"EnableInputHooks",1)' in main
import re
assert not re.search(r'shared->(?:ack|result)\s*=(?!=)',main)
cursor=(root/'cursor/source/main.cpp').read_text()
assert 'return external::discovery::isGameName(s);' in cursor
assert 'external::discovery::resolveYrppWindow(reader,pid,nullptr,&foundThread)' in cursor
assert 'static BOOL CALLBACK findWindow(' not in cursor
print('PASS: explicit feature/permission linkage, invalid-key isolation, shared cursor discovery and host ACK single-writer contract')

assert 'if(!foregroundGame()||snap.renderStatus!=1){ShowWindow(overlay,SW_HIDE);' in main
assert 'snap.renderStatus=externalValid&&d.renderStatus==1?1:0;' in main
monitor=(root/'unit_monitor/source/main.cpp').read_text() if (root/'unit_monitor/source/main.cpp').exists() else (root.parent/'unit_monitor/source/main.cpp').read_text()
assert 'external::display::inspect' in monitor and 'battlefield&&foregroundGame()' in monitor
print('PASS shared menu visibility is consumed by both overlays; control tick hides even when render clock stops')

assert 'tabEnabled&&enabled&&gameActive&&!demo' in monitor

assert 'if(c==RevealMap&&!extra.experimentalReveal)return false;' in main
assert 'if(command==RevealMap&&!extra.experimentalReveal)' in main
assert 'if(autoRevealNewMatch&&extra.experimentalReveal)' in main
assert 'commandQueue.slots[i].command==RevealMap' in main
assert 'case 116:toggleOverlay();' in monitor
assert 'if(a==1){toggleOverlay();}' in monitor
assert 'tabui::displayRequested(hidden,demo||!tabEnabled,manualVisible,tabs.visible())' in monitor
assert '常驻显示（不依赖 Tab）' in monitor
print('PASS experimental reveal default/roundtrip and all host gates; independent manual overlay controls retain battlefield gating')
