"""Compile actual experimental Host routes, settings IO and registration gates.

Win32 controls/INI are controlled seams. No game or live desktop is operated.
"""
from pathlib import Path
import argparse, hashlib, json, re, subprocess

HERE=Path(__file__).resolve().parent
SRC=HERE.parent/'source'

def block(text,needle):
    start=text.index(needle);at=text.index('{',start);depth=1;end=at+1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]

def generate():
    io=(SRC/'extra_settings_io.h').read_text(encoding='utf-8-sig')
    route=block((SRC/'auto_settings_ui.h').read_text(encoding='utf-8-sig'),'static bool autoSettingChanged(')
    option_route=block(route,' if(id>=9480&&id<=9485')+'\n'+block(route,' if(id==9486&&notification')
    main=(SRC/'main.cpp').read_text(encoding='utf-8-sig');hotkey=(SRC/'hotkey_host.h').read_text(encoding='utf-8-sig')
    load=block(io,'static void loadAdditionalSettings()');load=load[load.index(' extra.viewZoomEnabled='):load.index(' extra.techEnhance=')]
    save=block(io,'static bool saveAdditionalSettings()');save=save[save.index(' const wchar_t*gestureNames'):save.index(' write(L"Experimental",L"TechEnhance"')]
    sync=block(io,'static void syncAdditional()');sync=sync[sync.index(' shared->viewZoomPercent='):sync.index(' shared->buildTimeTooltip=')]
    route=route[route.index(' if((id==9090'):route.index(' if(id==886')]+'\n'+option_route
    register='\n'.join(l for l in main.splitlines() if l.startswith(' if(extra.sidebarEnabled&&LOBYTE') or l.startswith(' if(extra.viewZoomEnabled){for(int i=0;i<3;'))
    assert register.count('registerKey')==2
    hotkey=block(hotkey,' if(wp==1734&&extra.sidebarEnabled)')+'\n'+block(hotkey,' if(wp>=1720&&wp<=1722&&extra.viewZoomEnabled)')
    controls=(SRC/'tactical_controls.h').read_text(encoding='utf-8-sig')
    literal=[int(n) for n in re.findall(r'\b(?:toggle|button|number|edit|combo|key)\((94\d\d)\s*,',controls)]
    assert len(literal)==len(set(literal)),('duplicate new control IDs',literal)
    assert controls.count('toggle(9462,')==1 and controls.count('605,9460,')==1
    assert controls.count('toggle(9463,')==1 and controls.count('605,9464,')==1
    assert 'toggle(9460,' not in controls
    return r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include "additional_settings.h"
#include "runtime_feature_demand.h"
#include "view_zoom.h"
#include "../../suite/source/settings_categories.h"
using namespace assist;
static unsigned checks=0,changed=0,published=0,saved=0,notified=0;
#define assert(value) ((value)?(void)0:(std::fprintf(stderr,"assert %s at %d\n",#value,__LINE__),std::_Exit(3)))
static AdditionalSettings extra;
static std::map<std::pair<std::wstring,std::wstring>,int>ini;
static int value(const wchar_t*s,const wchar_t*k,int d){auto i=ini.find({s,k});return i==ini.end()?d:i->second;}
static bool write(const wchar_t*s,const wchar_t*k,int v){ini[{s,k}]=v;return true;}
static std::map<int,LRESULT>controlValues;
static HWND ui=nullptr;
static HWND fixtureItem(HWND,int id){return reinterpret_cast<HWND>(intptr_t(id));}
static LRESULT fixtureSend(HWND h,UINT message,WPARAM wp,LPARAM){int id=int(intptr_t(h));if(message==CB_SETCURSEL||message==BM_SETCHECK||message==HKM_SETHOTKEY){controlValues[id]=LRESULT(wp);return 0;}return controlValues[id];}
static LRESULT fixtureSendItem(HWND w,int id,UINT m,WPARAM p,LPARAM l){return fixtureSend(fixtureItem(w,id),m,p,l);}
static UINT fixtureNumber(HWND,int id,BOOL*ok,BOOL){*ok=controlValues.count(id)!=0;return UINT(controlValues[id]);}
static void setStatus(const std::wstring&){}
#define GetDlgItem fixtureItem
#define SendMessageW fixtureSend
#define SendDlgItemMessageW fixtureSendItem
#define GetDlgItemInt fixtureNumber
struct ControlUpdate{};
struct MatchState{int clears=0;void clear(){++clears;}}pointerMatches;
static void changedSetting(){++changed;}
static void notify(const std::wstring&){++notified;}
static std::map<int,WORD> registered;
static bool registerKey(int id,WORD key){if(LOBYTE(key))registered[id]=key;return true;}
static struct Wire{uint32_t viewZoomPercent=100,sidebarMode=0,smallMapClamp=0,pauseViewKeys[2]{},moFeatures=0;volatile long moGesturePolicySeq=0;moorders::GesturePolicy moGestures;moorders::Options moOptions;}wire,*shared=&wire;
static void registerKeys(){registered.clear();int failures=0;
''' + register + r'''
 assert(!failures);}
static void loadTargetedSettings(){
''' + load + r'''
}
static void saveTargetedSettings(){
''' + save + r'''
}
static bool save(){saveTargetedSettings();++saved;return true;}
static void syncAdditional(){if(!shared)return;++published;
''' + sync + r'''
}
static bool changeSetting(int id,int notification){const bool edit=notification==EN_CHANGE,select=notification==CBN_SELCHANGE;
''' + route + r'''
return false;}
static void executeExperimentalHotkey(WPARAM wp){
''' + hotkey + r'''
}
static void check(bool good){++checks;assert(good);}
static bool coldDemand(){return runtimeFeatureDemand(0,extra,false,false);}
int main(){
 loadTargetedSettings();registerKeys();syncAdditional();
 check(!extra.smallMapClamp&&!wire.smallMapClamp&&!extra.sidebarEnabled&&extra.sidebarMode==0&&!extra.viewZoomEnabled&&extra.viewZoomPercent==100&&!extra.moFeatures&&!coldDemand());
 check(registered.empty()&&extra.sidebarKey==VK_F11&&extra.pauseKeys[0]==VK_F5&&extra.pauseKeys[1]==VK_F6);
 check(moorders::validOptions(extra.moOptions)&&moorders::sameOptions(extra.moOptions,wire.moOptions));
 for(int id=9480;id<=9485;++id){controlValues[id]=id==9483?8:16;auto before=published;check(changeSetting(id,EN_CHANGE)&&published==before+1);}
 check(extra.moOptions.range==16&&extra.moOptions.squad==16&&extra.moOptions.patrolSize==16&&extra.moOptions.nodeStep==8&&extra.moOptions.loopClose==16&&extra.moOptions.loopStep==16);
 auto options=extra.moOptions;controlValues[9483]=9;check(changeSetting(9483,EN_CHANGE)&&moorders::sameOptions(options,extra.moOptions));
 controlValues[9486]=BST_UNCHECKED;check(changeSetting(9486,BN_CLICKED)&&!wire.moOptions.commit);controlValues[9486]=BST_CHECKED;check(changeSetting(9486,BN_CLICKED)&&wire.moOptions.commit);
 saveTargetedSettings();extra.moOptions={};loadTargetedSettings();check(moorders::sameOptions(extra.moOptions,options));
 // Independent opt-in must connect even while its display mode is full.
 controlValues[9462]=BST_CHECKED;check(changeSetting(9462,BN_CLICKED));check(extra.sidebarEnabled&&extra.sidebarMode==0&&coldDemand()&&registered.count(1734)&&wire.sidebarMode==0);
 controlValues[9404]=1;check(changeSetting(9404,CBN_SELCHANGE));check(extra.sidebarMode==1&&wire.sidebarMode==1);
 executeExperimentalHotkey(1734);check(extra.sidebarMode==2&&wire.sidebarMode==2&&coldDemand());executeExperimentalHotkey(1734);check(extra.sidebarMode==0&&wire.sidebarMode==0);
 controlValues[9462]=BST_UNCHECKED;check(changeSetting(9462,BN_CLICKED));check(!registered.count(1734)&&wire.sidebarMode==0&&!coldDemand());auto old=changed;executeExperimentalHotkey(1734);check(extra.sidebarMode==0&&changed==old&&wire.sidebarMode==0);
 // Unrelated MO status id must not be interpreted as a checkbox.
 check(!changeSetting(9460,BN_CLICKED));
 // Small-map clamping is independent from zoom/sidebar and needs Runtime
 // even with no external display sample or other active features.
 controlValues[9463]=BST_CHECKED;check(changeSetting(9463,BN_CLICKED));check(extra.smallMapClamp&&wire.smallMapClamp&&coldDemand()&&!extra.sidebarEnabled&&!extra.viewZoomEnabled);
 saveTargetedSettings();extra.smallMapClamp=false;loadTargetedSettings();syncAdditional();check(extra.smallMapClamp&&wire.smallMapClamp&&coldDemand());
 controlValues[9463]=BST_UNCHECKED;check(changeSetting(9463,BN_CLICKED));check(!extra.smallMapClamp&&!wire.smallMapClamp&&!coldDemand());saveTargetedSettings();
 // Every ratio is routed from the real control and publishes the value.
 controlValues[9090]=BST_CHECKED;check(changeSetting(9090,BN_CLICKED));check(coldDemand()&&registered.count(1720)&&registered.count(1721)&&registered.count(1722));
 for(int i=0;i<viewzoom::Count;++i){controlValues[9091]=i;check(changeSetting(9091,CBN_SELCHANGE));check(extra.viewZoomPercent==viewzoom::Levels[i]&&wire.viewZoomPercent==uint32_t(viewzoom::Levels[i]));}
 executeExperimentalHotkey(1720);check(extra.viewZoomPercent==300&&wire.viewZoomPercent==300);executeExperimentalHotkey(1721);check(extra.viewZoomPercent==400);executeExperimentalHotkey(1721);check(extra.viewZoomPercent==400);executeExperimentalHotkey(1722);check(extra.viewZoomPercent==100&&wire.viewZoomPercent==100);
 controlValues[9091]=0;check(changeSetting(9091,CBN_SELCHANGE));executeExperimentalHotkey(1720);check(extra.viewZoomPercent==0);check(changeSetting(9092,BN_CLICKED)&&extra.viewZoomPercent==100);
 controlValues[9090]=BST_UNCHECKED;check(changeSetting(9090,BN_CLICKED));check(!coldDemand()&&!registered.count(1720)&&wire.viewZoomPercent==100);old=changed;executeExperimentalHotkey(1721);check(extra.viewZoomPercent==100&&changed==old);
 // Six configurable hotkeys write the correct independent slot and republish.
 for(int id=9450;id<=9455;++id){controlValues[id]=('A'+id-9450)|(6<<8);auto before=published;check(changeSetting(id,EN_CHANGE)&&published==before+1);uint32_t expected=uint32_t(controlValues[id]);check(id<=9452?extra.viewZoomKeys[id-9450]==expected:id==9453?extra.sidebarKey==expected:extra.pauseKeys[id-9454]==expected);}
 controlValues[9403]=BST_CHECKED;check(changeSetting(9403,BN_CLICKED));check((extra.moFeatures&16)&&coldDemand()&&wire.moFeatures==16&&wire.pauseViewKeys[0]==extra.pauseKeys[0]);
 controlValues[9403]=BST_UNCHECKED;check(changeSetting(9403,BN_CLICKED));check(!extra.moFeatures&&!coldDemand()&&!wire.moFeatures);
 // Save and load exact dormant/active settings; only the effective enabled
 // state crosses IPC. Reserved bits and invalid ratios never become enabled.
 extra.sidebarEnabled=true;extra.sidebarMode=0;extra.viewZoomEnabled=true;extra.viewZoomPercent=125;extra.moFeatures=23;auto before=extra;saveTargetedSettings();extra={};loadTargetedSettings();
 check(extra.sidebarEnabled&&extra.sidebarMode==0&&extra.viewZoomEnabled&&extra.viewZoomPercent==125&&extra.moFeatures==23);
 for(int i=0;i<3;++i)check(extra.viewZoomKeys[i]==before.viewZoomKeys[i]);for(int i=0;i<2;++i)check(extra.pauseKeys[i]==before.pauseKeys[i]);check(extra.sidebarKey==before.sidebarKey);
 ini[{L"Experimental",L"SidebarEnabled"}]=0;ini[{L"Experimental",L"SidebarMode"}]=2;ini[{L"ViewZoom",L"Enabled"}]=0;ini[{L"ViewZoom",L"Percent"}]=51;ini[{L"MOControls",L"Features"}]=31;loadTargetedSettings();syncAdditional();
 check(!extra.sidebarEnabled&&extra.sidebarMode==2&&wire.sidebarMode==0&&!extra.viewZoomEnabled&&extra.viewZoomPercent==100&&wire.viewZoomPercent==100&&extra.moFeatures==23);
 ini[{L"Experimental",L"SidebarMode"}]=1;loadTargetedSettings();check(extra.sidebarMode==1);
 // All requested controls and their labels/status stay in Experimental;
 // external superweapon panel retains its separate Video classification.
 for(int id:{9090,9091,9092,9093,9403,9404,9406,9407,9450,9451,9452,9453,9454,9455,9462,9463,9464})check(suite::categories::row(4,&id,1)==suite::categories::Experimental);
 int id=9405;check(suite::categories::row(4,&id,1)==suite::categories::Video);id=9460;check(suite::categories::row(1,&id,1)==suite::categories::Control);
 std::printf("{\"checks\":%u,\"assertions_active\":true,\"pass\":true}\n",checks);
}
'''

def main():
    p=argparse.ArgumentParser();p.add_argument('--zig',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    cpp=a.out/'experimental-view-host.cpp';cpp.write_text(generate(),encoding='utf-8');result={'scope':'Actual extracted Host UI, IO, IPC, hotkey routes and registration gates; no live game','architectures':{}}
    for arch in ('x86_64','x86'):
        exe=a.out/(arch+'.exe');cmd=[str(a.zig),'c++','-std=c++17','-O2','-UNDEBUG','-target',arch+'-windows-gnu','-I'+str(SRC),str(cpp),'-o',str(exe)]
        build=subprocess.run(cmd,capture_output=True,text=True);(a.out/(arch+'-build.log')).write_text(build.stdout+build.stderr,encoding='utf-8');build.check_returncode()
        run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30);(a.out/(arch+'.log')).write_text(run.stdout+run.stderr,encoding='utf-8');print(run.stdout+run.stderr,end='');run.check_returncode();result['architectures'][arch]=json.loads(run.stdout)
    names=['additional_settings.h','extra_settings_io.h','auto_settings_ui.h','main.cpp','hotkey_host.h','runtime_feature_demand.h','tactical_controls.h']
    result['source_sha256']={n:hashlib.sha256((SRC/n).read_bytes()).hexdigest() for n in names};result['source_sha256']['suite/source/settings_categories.h']=hashlib.sha256((HERE.parents[1]/'suite/source/settings_categories.h').read_bytes()).hexdigest()
    (a.out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result,indent=2))

if __name__=='__main__':main()
