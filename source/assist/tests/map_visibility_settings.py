"""Execute production map switch/INI/Shared and hardware admission with UI seams."""
import argparse, hashlib, json, re, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
src=Path(__file__).resolve().parents[1]/'source'
ui=(src/'auto_settings_ui.h').read_text();io=(src/'extra_settings_io.h').read_text();host=(src/'main.cpp').read_text();deps=(src/'ui_dependencies.h').read_text()
def block(s,key):
 start=s.index(key);at=s.index('{',start)+1;depth=1
 while depth:depth+=(s[at]=='{')-(s[at]=='}');at+=1
 return s[start:at]
branches='\n'.join(block(ui,key)for key in ['if(id==9492&&notification==BN_CLICKED)','if(id==808&&select&&extra.mapVisibility','if(id==9199&&notification==BN_CLICKED&&extra.mapVisibility'])
load=re.search(r'extra\.mapVisibility=[^;]+;',io)[0];save=re.search(r'write\(L"Experimental",L"HardwareMapVisibility",[^;]+;',io)[0];sync=re.search(r'shared->mapVisibility=[^;]+;',io)[0]
cpp=r'''
#include "runtime_feature_demand.h"
#include "settings_dependencies.h"
#include "shroud_policy.h"
#include "../../shared/source/hardware_slot_budget.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
using namespace assist;
static AdditionalSettings extra;static Shared wire,*shared=&wire;
static int checked=0,selected=0,changes=0,syncs=0,notices=0,ui=1;static bool techMode=false;static int techImplementation=buildingmodes::TechParameters;static uint32_t budget=0;
static constexpr int BN_CLICKED=0,BM_GETCHECK=1,BM_SETCHECK=2,CB_GETCURSEL=3,CB_SETCURSEL=4,BST_CHECKED=1,BST_UNCHECKED=0;
static std::map<std::pair<std::wstring,std::wstring>,int> ini;
static int value(const wchar_t*s,const wchar_t*k,int d){auto i=ini.find({s,k});return i==ini.end()?d:i->second;}
static void write(const wchar_t*s,const wchar_t*k,int v){ini[{s,k}]=v;}
static void load(){LOAD}static void save(){SAVE}
static void syncAdditional(){SYNC ++syncs;}static void changedSetting(){++changes;save();}
struct ControlUpdate{};static void notify(const wchar_t*){++notices;}
static int GetDlgItem(int,int id){return id;}
static int SendMessageW(int,int message,int v,int){switch(message){case BM_GETCHECK:return checked;case BM_SETCHECK:checked=v;break;case CB_GETCURSEL:return selected;case CB_SETCURSEL:selected=v;break;}return 0;}
static uint32_t currentHardwareBudget(){return budget;}
TAGS
AVAILABLE
static settingsdeps::State state(){settingsdeps::State s;BLOCKED return s;}
static bool route(int id,int notification,bool select=false){BRANCHES return false;}
static bool click(int on){checked=on;if(!settingsdeps::decision(9492,state()).enabled()){checked=extra.mapVisibility;return false;}return route(9492,BN_CLICKED);}
int main(){unsigned checks=0;auto check=[&](bool b){assert(b);++checks;};
 load();syncAdditional();check(!extra.mapVisibility&&!wire.mapVisibility&&!runtimeFeatureDemand(0,extra,false,false));
 check(!click(1)&&!extra.mapVisibility); // Unknown budget denies enable.
 budget=slotbudget::pack(4,0,0,0);check(click(1)&&extra.mapVisibility&&wire.mapVisibility&&requestedHardwareTags()==slotbudget::Map);check(runtimeFeatureDemand(0,extra,false,false));
 budget=slotbudget::pack(0,4,0,23);check(click(0)&&!extra.mapVisibility&&!wire.mapVisibility); // Disable remains available.
 extra.controlAll=true;extra.techEnhance=true;extra.techEnhanceImplementation=1;techMode=true;
 budget=slotbudget::pack(1,3,0,21);check(requestedHardwareTags()==21&&hardwareBudgetAvailable(slotbudget::Map));check(click(1)&&requestedHardwareTags()==23);
 extra.techEnhanceCompareLevel=true;check(!hardwareBudgetAvailable(slotbudget::Compare));extra.techEnhanceCompareLevel=false;
 check(click(0));extra.techEnhanceCompareLevel=true;budget=slotbudget::pack(0,4,0,29);check(!click(1)&&!extra.mapVisibility&&requestedHardwareTags()==29);
 budget=slotbudget::pack(0,3,1,21);extra.techEnhanceCompareLevel=false;check(!click(1));
 extra=AdditionalSettings{};techMode=false;budget=slotbudget::pack(4,0,0,0);
 for(int strategy:{1,2,3,4,7,8}){extra.mapStrategy=strategy;const int c=changes;check(click(1)&&!extra.mapVisibility&&!checked&&changes==c);}
 extra.mapStrategy=0;extra.manualReveal=true;check(click(1)&&!extra.mapVisibility&&!checked);extra.manualReveal=false;check(click(1)&&extra.mapVisibility);
 selected=2;check(route(808,1,true)&&selected==0&&extra.mapVisibility&&extra.mapStrategy==0);checked=1;check(route(9199,0)&&checked==0&&!extra.manualReveal);
 check(click(0));extra.mapVisibility=true;save();extra.mapVisibility=false;load();syncAdditional();check(extra.mapVisibility&&wire.mapVisibility);
 check(!route(9491,BN_CLICKED)&&!route(9492,9));check(notices==9);check(Version==96);
 std::printf("PASS map settings/admission checks=%u\n",checks);
}
'''
parts={'LOAD':load,'SAVE':save,'SYNC':sync,'TAGS':block(host,'static uint32_t requestedHardwareTags()'),'AVAILABLE':block(host,'static bool hardwareBudgetAvailable('),'BLOCKED':re.search(r'if\(!extra\.mapVisibility[^\n]+',deps)[0],'BRANCHES':branches}
for k,v in parts.items():cpp=cpp.replace(k,v)
cp=a.out/'settings.cpp';exe=a.out/'settings';cp.write_text(cpp)
subprocess.run(['g++','-std=c++17','-O2','-UNDEBUG','-I'+str(src),str(cp),'-o',str(exe)],check=True)
r=subprocess.run([str(exe)],capture_output=True,text=True);print(r.stdout,end='');assert r.returncode==0,r.stderr
files=['additional_settings.h','auto_settings_ui.h','extra_settings_io.h','main.cpp','ui_dependencies.h','settings_dependencies.h','runtime_feature_demand.h','protocol.h']
(a.out/'result.json').write_text(json.dumps(dict(pass_=True,scope=__doc__,stdout=r.stdout,real_windows_ui=False,source_sha256={n:hashlib.sha256((src/n).read_bytes()).hexdigest()for n in files}),indent=2)+'\n')
