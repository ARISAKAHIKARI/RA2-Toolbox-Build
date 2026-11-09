#!/usr/bin/env python3
"""Focused automatic-control tests using production handlers and input routing.

Reuses only the portable Win32 boundary preludes from the existing UI/dispatcher
fixtures; their unrelated test bodies are not run. Compiles the complete shipped
settings handler and dispatcher, plus extracted owner branches/registration.
No Windows/game process or rendered browser is exercised.
"""
from pathlib import Path
import argparse, hashlib, json, re, subprocess, sys

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--out',type=Path,required=True)
args=parser.parse_args()
root=Path(__file__).resolve().parents[2]
repo=root.parent
out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
production=root/'assist/source'
main=(production/'main.cpp').read_text()
settings=(production/'auto_settings_ui.h').read_text()
conflict=(production/'type_bindings_ui.h').read_text().split('static WORD comparableKey',1)[1].split('static void syncTypeConfig()',1)[0]

# Load the existing fixture's boundary prelude, never its old test body.
ui_fixture=root/'assist/tests/auto_settings_ui_test.py'
prelude=ui_fixture.read_text().split("suffix=r'''",1)[0]
prelude=re.sub(r'^root=Path\(sys.argv\[1\]\).*$', 'root=Path(__source_root__)',prelude,flags=re.M)
env={'__file__':str(ui_fixture),'__source_root__':str(root)}
exec(compile(prelude,str(ui_fixture),'exec'),env)
prefix=env['prefix']
prefix=prefix.replace('#include "additional_settings.h"','#include "additional_settings.h"\n#include "settings_dependencies.h"\n#include "single_player_fps.h"\n#include "hotkey_routes.h"\n#include <iostream>\n#include <set>')
prefix=prefix.replace('using HWND=int;','using HWND=intptr_t;')
prefix=prefix.replace('static bool save(){++saves;return autoSaveOK;}','static bool save();')
assignment=re.search(r'shared->repairMs=repairMs;', (production/'type_bindings_ui.h').read_text()).group()
prefix=prefix.replace('static void syncTypeConfig(){++syncs;}','static struct {int repairMs=0;} runtimeConfig;static auto*shared=&runtimeConfig;\nstatic void syncTypeConfig(){++syncs;'+assignment+'}')
old='static bool keyConflict(WORD next,const std::string&,bool){if(extra.allHotkeys||!next)return false;for(auto k:hotkeys)if(k==next)return true;for(auto k:{csKey,advancedKey,unloadKey,ejectKey,csHoldKey,magnetronKey,idleAircraftKey})if(k==next)return true;return false;}'
assert old in prefix
prefix=prefix.replace(old,'static unsigned LOBYTE(uintptr_t k){return k&255;}\nstatic std::vector<Binding>typeBindings;\nstatic WORD comparableKey'+conflict)
boundaries=r'''
static bool buildTimeTooltipEnabled=false,harvesterCounterEnabled=false,powerDeltaEnabled=false,renderHighRefresh=true;
static struct {bool detached=false,collapsed=false;int x=20,y=150,scale=100,rows=8;}superweaponOptions;
static void setConsoleFeedback(HWND,const std::wstring&){}static void wakeGame(){}
static void resetPlacementEvent(){}static void updatePlacementEventUI(){}static void selectPlacementEvent(){}
static bool hardwareBudgetAvailable(unsigned){return true;}static std::wstring hardwareBudgetReason(unsigned){return L"";}
'''
prefix=prefix.replace('static void loadMouse(){',boundaries+'\nstatic void loadMouse(){',1)
ini=re.search(r'static const char\* iniKeys\[\]=\{[^\n]+',main).group()
legacy_load=re.search(r' const WORD defaults\[\]=\{[^\n]+',main).group()
legacy_save=re.search(r'for\(int i=0;i<12;\+\+i\)write\(L"Hotkeys",[^;]+;',main).group()
repair_save=re.search(r'write\(L"Modes",L"RepairMs",repairMs\);',main).group()
repair_load=re.search(r'repairMs=std::clamp\(value\(L"Modes",L"RepairMs",2000\),500,30000\);',main).group()
new_save=re.search(r' autofeatures::saveKeys\([^\n]+',(production/'extra_settings_io.h').read_text()).group()
prefix+='\n'+ini+r'''
static std::wstring wide(const char*s){std::wstring result;while(*s)result+=wchar_t(*s++);return result;}
static bool save(){++saves;
'''+legacy_save+repair_save+new_save+r'''
 return autoSaveOK;
}
static void loadLegacyKeys(){
'''+legacy_load+repair_load+r'''
}
using WPARAM=uintptr_t;using LPARAM=intptr_t;using UINT=unsigned;using LRESULT=intptr_t;
constexpr UINT WM_COMMAND=273;
static WPARAM MAKEWPARAM(unsigned a,unsigned b){return a|(b<<16);}
static unsigned LOWORD(uintptr_t v){return v&65535;}static unsigned HIWORD(uintptr_t v){return (v>>16)&65535;}
static bool hookEject=false,hookAutoCS=false,ejectMode=false,csMode=false,oreMode=false;
static int repairCheck=100,ejectCheck=455,csCheck=446,oreCheck=470;
static std::set<int>disabledControls;
static bool IsWindowEnabled(HWND control){settingsdeps::State state;state.set(settingsdeps::Need::Missile,extra.missileDefense);state.set(settingsdeps::Need::Repair,fields[100]==BST_CHECKED);return !disabledControls.count(int(control))&&settingsdeps::decision(int(control),state).enabled();}
static LRESULT proc(HWND,UINT,WPARAM,LPARAM);
#include "auto_settings_ui.h"
#include "automatic_hotkeys_host.h"
static unsigned ownerCalls=0;
static LRESULT proc(HWND,UINT message,WPARAM wp,LPARAM){
 assert(message==WM_COMMAND);++ownerCalls;const int id=LOWORD(wp);
 if(autoSettingChanged(id,HIWORD(wp)))return 0;
'''
# These existing feature mutations are the actual UI owner branches.
for control in (446,455,470):
    line=next(s.strip() for s in main.splitlines() if s.lstrip().startswith(f'else if(id=={control})'))
    prefix+=line.removeprefix('else ')+'\n'
repair_branch=main.split('if(id>=100&&id<112){',1)[1].split('\n }',1)[0]
# Repair is the only persistent automatic switch in the old command button row.
repair_body=re.search(r'if\(id==100\)\{([^}]+)\}',repair_branch).group(1)
prefix+=' if(id==100){'+repair_body+'}\n return 0;\n}\n'
ui_tests=r'''
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";std::abort();}}while(0)
static bool featureValue(autofeatures::Feature feature){switch(feature){
 case autofeatures::Repair:return fields[100]==BST_CHECKED;case autofeatures::Eject:return ejectMode;case autofeatures::OreReturn:return oreMode;
 case autofeatures::AutoCS:return csMode;case autofeatures::MagnetronDrop:return extra.magnetronDrop;case autofeatures::ConstructionQueue:return extra.constructionQueue;
 case autofeatures::BombBoard:return extra.bombBoardEnabled;case autofeatures::MinerDefense:return extra.minerDefenseEnabled;case autofeatures::AntiTheft:return extra.antiTheft;case autofeatures::Dodge:return extra.dodgeEnabled;
 case autofeatures::MissileDefense:return extra.missileDefense;case autofeatures::MissileReserve:return extra.missileReserve;case autofeatures::LowDamageFilter:return extra.lowDamageFilter;
 case autofeatures::MagnetronGlobal:return extra.magnetronGlobal;default:std::abort();}}
int main(){
 std::set<int>controls,keys,actions;std::set<std::wstring>names;unsigned newKeys=0;
 CHECK(autofeatures::Count==14);
 for(const auto&e:autofeatures::Entries){CHECK(controls.insert(e.control).second);CHECK(keys.insert(e.keyControl).second);CHECK(actions.insert(e.action).second);CHECK(autofeatures::byControl(e.control)==&e);CHECK(autofeatures::byKeyControl(e.keyControl)==&e);CHECK(autofeatures::byAction(e.action)==&e);if(e.setting){++newKeys;CHECK(names.insert(e.setting).second);CHECK(extra.automaticKeys[e.feature]==0);}}
 CHECK(newKeys==11);CHECK(!autofeatures::byControl(111));CHECK(!autofeatures::byAction(1011));CHECK(!autofeatures::byControl(312));CHECK(!autofeatures::byControl(781));CHECK(!autofeatures::byControl(9200));
 loadLegacyKeys();CHECK(hotkeys[0]==MAKEWORD('R',6));CHECK(hotkeys[11]==MAKEWORD('L',6));CHECK(repairMs==2000);
 profile[L"Repair"]=MAKEWORD('R',2);profile[L"AutoLoad"]=MAKEWORD('L',2);loadLegacyKeys();CHECK(hotkeys[0]==MAKEWORD('R',2));CHECK(hotkeys[11]==MAKEWORD('L',2));
 loadFeatureTypes();for(const auto&e:autofeatures::Entries)if(e.setting)CHECK(extra.automaticKeys[e.feature]==0);
 // Every supported automatic switch uses the owner's actual click handler.
 for(const auto&e:autofeatures::Entries){
  if(e.feature==autofeatures::AutoCS)continue;
  if(e.feature==autofeatures::MissileReserve)extra.missileDefense=true;
  fields[e.control]=BST_UNCHECKED;proc(ui,WM_COMMAND,MAKEWPARAM(e.control,BN_CLICKED),e.control);CHECK(!featureValue(e.feature));
  auto before=ownerCalls;CHECK(automaticHotkey(e.action));CHECK(ownerCalls==before+1);CHECK(fields[e.control]==BST_CHECKED);CHECK(featureValue(e.feature));
  CHECK(automaticHotkey(e.action));CHECK(!featureValue(e.feature));CHECK(fields[e.control]==BST_UNCHECKED);
  fields[e.control]=BST_CHECKED;proc(ui,WM_COMMAND,MAKEWPARAM(e.control,BN_CLICKED),e.control);CHECK(featureValue(e.feature));
 }
 CHECK(ejectMode==hookEject);
 fields[446]=BST_UNCHECKED;auto beforeOwner=ownerCalls;CHECK(automaticHotkey(1699));CHECK(ownerCalls==beforeOwner&&!csMode&&fields[446]==BST_UNCHECKED);
 extra.missileDefense=false;bool reserve=extra.missileReserve;CHECK(automaticHotkey(1809));CHECK(extra.missileReserve==reserve&&ownerCalls==beforeOwner);extra.missileDefense=true;
 disabledControls.insert(899);bool drop=extra.magnetronDrop;CHECK(automaticHotkey(1802));CHECK(extra.magnetronDrop==drop);disabledControls.clear();
 CHECK(!automaticHotkey(1011));CHECK(!automaticHotkey(777));
 ready=false;bool dodge=extra.dodgeEnabled;CHECK(automaticHotkey(1807));CHECK(extra.dodgeEnabled==dodge);ready=true;
 // New bindings are editable while their feature is off, retain gestures and
 // reject conflicts in both directions without enabling the feature.
 extra.allHotkeys=false;extra.dodgeEnabled=false;fields[885]=BST_UNCHECKED;
 for(auto&k:hotkeys)k=0;csKey=advancedKey=unloadKey=ejectKey=csHoldKey=magnetronKey=idleAircraftKey=0;extra.variantKey=extra.placementEventKey=0;
 fields[9507]=MAKEWORD('X',2)|0x2000;CHECK(autoSettingChanged(9507,EN_CHANGE));CHECK(extra.automaticKeys[autofeatures::Dodge]==unsigned(fields[9507]));CHECK(!extra.dodgeEnabled&&fields[885]==BST_UNCHECKED);CHECK(autoSavePending&&timerMs==300);
 auto prior=extra.automaticKeys[autofeatures::Dodge];fields[9502]=prior;CHECK(autoSettingChanged(9502,EN_CHANGE));CHECK(extra.automaticKeys[autofeatures::MagnetronDrop]==0&&fields[9502]==0);
 fields[200]=prior;CHECK(autoSettingChanged(200,EN_CHANGE));CHECK(hotkeys[0]==0&&fields[200]==0);
 fields[9502]=MAKEWORD('R',6);CHECK(autoSettingChanged(9502,EN_CHANGE));CHECK(extra.automaticKeys[autofeatures::MagnetronDrop]==unsigned(fields[9502]));
 hotkeys[11]=MAKEWORD('L',6);fields[9507]=hotkeys[11];CHECK(autoSettingChanged(9507,EN_CHANGE));CHECK(extra.automaticKeys[autofeatures::Dodge]==prior&&fields[9507]==int(prior));
 // Existing registered commands must retain their key when a new automatic
 // binding is edited. Check every route formerly missing from keyConflict.
 auto occupied=[&](uint32_t&existing){const uint32_t old=existing;existing='K';fields[9507]='K';const auto reg=registrations;CHECK(autoSettingChanged(9507,EN_CHANGE));CHECK(extra.automaticKeys[autofeatures::Dodge]==prior&&fields[9507]==int(prior));CHECK(registrations==reg);existing=old;};
 occupied(extra.stickyKey);
 for(auto&binding:extra.missionModeKeys)occupied(binding);
 extra.viewZoomEnabled=true;for(auto&binding:extra.viewZoomKeys)occupied(binding);extra.viewZoomEnabled=false;
 extra.sidebarEnabled=true;occupied(extra.sidebarKey);extra.sidebarEnabled=false;
 extra.watchEnabled=false;extra.watchSecondary=true;for(auto&binding:extra.watchKeys)occupied(binding);extra.watchSecondary=false;
 // The same physical key with a different gesture is allowed; explicit
 // Press and legacy Press are equivalent. Fanout remains an explicit option.
 extra.stickyKey='K';fields[9507]='K'|0x2000;CHECK(autoSettingChanged(9507,EN_CHANGE));CHECK(extra.automaticKeys[autofeatures::Dodge]==unsigned('K'|0x2000));
 fields[9507]='K'|0x1000;CHECK(autoSettingChanged(9507,EN_CHANGE));CHECK(extra.automaticKeys[autofeatures::Dodge]==unsigned('K'|0x2000));
 extra.allHotkeys=true;fields[9507]='K';CHECK(autoSettingChanged(9507,EN_CHANGE));CHECK(extra.automaticKeys[autofeatures::Dodge]=='K');extra.allHotkeys=false;
 extra.stickyKey=0;fields[9507]=prior;CHECK(autoSettingChanged(9507,EN_CHANGE));CHECK(extra.automaticKeys[autofeatures::Dodge]==prior);
 // The reverse edit cannot displace the automatic binding either.
 auto reverseReg=registrations;fields[9095]=prior;CHECK(autoSettingChanged(9095,EN_CHANGE));CHECK(extra.stickyKey==0&&fields[9095]==0&&registrations==reverseReg);CHECK(extra.automaticKeys[autofeatures::Dodge]==prior);
 fields[9095]=MAKEWORD('X',2);CHECK(autoSettingChanged(9095,EN_CHANGE));CHECK(extra.stickyKey==MAKEWORD('X',2));
 fields[9095]=0;CHECK(autoSettingChanged(9095,EN_CHANGE));CHECK(extra.stickyKey==0);
 extra.allHotkeys=true;fields[9095]=prior;CHECK(autoSettingChanged(9095,EN_CHANGE));CHECK(extra.stickyKey==prior);extra.allHotkeys=false;extra.stickyKey=0;
 extra.watchKeys[0]=extra.watchKeys[1]=extra.watchKeys[2]='W';fields[9063]='W';CHECK(autoSettingChanged(9063,EN_CHANGE));CHECK(extra.watchKeys[0]=='W'&&extra.watchKeys[1]=='W'&&extra.watchKeys[2]=='W');
 extra.allHotkeys=true;fields[9502]=prior;CHECK(autoSettingChanged(9502,EN_CHANGE));CHECK(extra.automaticKeys[autofeatures::MagnetronDrop]==prior);CHECK(destructOwnerKeyConflict(WORD(prior)));extra.allHotkeys=false;
 fields[9502]=0;CHECK(autoSettingChanged(9502,EN_CHANGE));CHECK(!extra.automaticKeys[autofeatures::MagnetronDrop]);
 auto beforeReg=registrations;updatingControls=true;fields[9507]='Y';CHECK(!autoSettingChanged(9507,EN_CHANGE));CHECK(extra.automaticKeys[autofeatures::Dodge]==prior&&registrations==beforeReg);updatingControls=false;
 flushAutoSettings();CHECK(!autoSavePending&&profile[L"Dodge"]==int(prior));extra.automaticKeys[autofeatures::Dodge]=0;loadFeatureTypes();CHECK(extra.automaticKeys[autofeatures::Dodge]==prior);CHECK(hotkeys[11]==MAKEWORD('L',6));
 // Both native and WebView adapters notify this exact EN_CHANGE path. Disk
 // persistence is debounced; repairMs reaches shared config before the flush.
 for(int value:{500,800,2000,30000}){fields[312]=value;auto oldSaves=saves;CHECK(autoSettingChanged(312,EN_CHANGE));CHECK(repairMs==value&&runtimeConfig.repairMs==value);CHECK(saves==oldSaves&&autoSavePending);flushAutoSettings();CHECK(profile[L"RepairMs"]==value);}
 for(int invalid:{0,499,30001,60000}){const int old=repairMs;auto oldSyncs=syncs,oldSaves=saves;fields[312]=invalid;CHECK(autoSettingChanged(312,EN_CHANGE));CHECK(repairMs==old&&runtimeConfig.repairMs==old&&syncs==oldSyncs&&saves==oldSaves);CHECK(lastMessage.find(L"500")!=std::wstring::npos&&lastMessage.find(L"30000")!=std::wstring::npos);}
 fields.erase(312);CHECK(autoSettingChanged(312,EN_CHANGE));CHECK(repairMs==30000);
 profile[L"RepairMs"]=5;loadLegacyKeys();CHECK(repairMs==500);profile[L"RepairMs"]=40000;loadLegacyKeys();CHECK(repairMs==30000);
 std::cout<<"PASS automatic UI/settings checks="<<checks<<"\n";
}
'''

# Reuse the current production-context harness: the legacy display-gated
# dispatcher fixture intentionally predates the command-channel recovery.
import hotkey_command_context as current_hotkeys
hot_fixture=Path(current_hotkeys.__file__)
route=current_hotkeys.program(main,(production/'hotkey_host.h').read_text()).split(current_hotkeys.TESTS,1)[0]
route=route.replace('static bool automaticHotkey(WPARAM){return false;}', 'static std::vector<int> automaticEvents;static bool automaticHotkey(WPARAM action){if(!autofeatures::byAction(int(action)))return false;automaticEvents.push_back(int(action));return true;}')
route+=current_hotkeys.TESTS.split('int main(int argc,char**argv){',1)[0]
route+=r'''
int main(){
 // Every feature, including BombBoard, has an independently usable OFF-state
 // shortcut even when optional rendering is unavailable.
 for(int render:{0,1}){
  reset(render);hotkeys[11]=0;extra.placementEventKey=0;
  for(const auto&e:autofeatures::Entries){WORD k=WORD(0x641+e.feature);if(e.feature==autofeatures::Repair)hotkeys[0]=k;else if(e.feature==autofeatures::Eject)ejectKey=k;else if(e.feature==autofeatures::AutoCS)csKey=k;else extra.automaticKeys[e.feature]=k;}
  registerKeys();check(hotkeyRegistry.groups.size()==13,"one registration per active feature; retired AutoCS excluded");
  for(const auto&e:autofeatures::Entries){if(e.feature==autofeatures::AutoCS)continue;auto*g=hotkeyRegistry.find(e.action);check(g&&g->actions.size()==1,"feature registration");auto before=automaticEvents.size();const WORD k=WORD(0x641+e.feature);++now;messageAt=now;handleHotkey(e.action,LPARAM((LOBYTE(k)<<16)|(modifiers(k)&~MOD_NOREPEAT)));check(automaticEvents.size()==before+1&&automaticEvents.back()==e.action,"dispatch into automatic owner");}
  check(commands.empty(),"automatic switches must not dispatch a selected-unit order");
  auto before=automaticEvents.size();focused=false;++now;messageAt=now;handleHotkey(1813,LPARAM(('N'<<16)|MOD_CONTROL|MOD_ALT));check(automaticEvents.size()==before,"no action outside game focus");
 }
 // Preserve the ordinary selected-unit action with the real command sink.
 reset();registerKeys();message();expectOneAutoBoard("AutoBoard remains one selected-unit command");
 std::cout<<"PASS automatic hotkey registration/context checks="<<checks<<"\n";
}
'''

def run(name,program,defines=()):
    cpp=out/(name+'.cpp');exe=out/name;cpp.write_text(program)
    command=['g++','-std=c++17','-O0',*defines,'-I'+str(production),str(cpp),'-o',str(exe)]
    built=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (out/(name+'-build.log')).write_text(built.stdout)
    if built.returncode:print(built.stdout[-16000:]);raise SystemExit(built.returncode)
    result=subprocess.run([str(exe)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (out/(name+'.log')).write_text(result.stdout);print(result.stdout,end='')
    if result.returncode:raise SystemExit(result.returncode)
    return {'passed':True,'checks':int(re.search(r'checks=(\d+)',result.stdout).group(1)), 'log':str(out/(name+'.log'))}
results={}
# Save a reproducible counterexample against the immediately preceding
# production conflict function, if that reviewed snapshot is supplied.
before=out/'before-registration-conflict-fix.h'
negative=None
if before.exists():
    old_conflict=before.read_text().split('static WORD comparableKey',1)[1].split('static void syncTypeConfig()',1)[0]
    # Only the forward registration-conflict list is the baseline. The newly
    # added reverse editor guard remains current so its helper is available.
    if 'static bool automaticKeyConflict(' not in old_conflict:
        helper='static bool automaticKeyConflict('+conflict.split('static bool automaticKeyConflict(',1)[1].split('static bool keyConflict(',1)[0]
        old_conflict=old_conflict.replace('static bool keyConflict(',helper+'static bool keyConflict(',1)
    negative_program=(prefix+ui_tests).replace('static WORD comparableKey'+conflict,'static WORD comparableKey'+old_conflict,1)
    cpp=out/'before-conflict-ui.cpp';exe=out/'before-conflict-ui';cpp.write_text(negative_program)
    build=subprocess.run(['g++','-std=c++17','-O0','-I'+str(production),str(cpp),'-o',str(exe)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if build.returncode:raise RuntimeError('Baseline counterexample did not compile: '+build.stdout)
    result=subprocess.run([str(exe)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (out/'before-conflict-ui.log').write_text(result.stdout)
    if result.returncode==0 or 'extra.automaticKeys[autofeatures::Dodge]==prior&&fields[9507]==int(prior)' not in result.stdout:raise RuntimeError('Expected sticky-key conflict counterexample was not reproduced')
    negative={'baseline_function_sha256':hashlib.sha256(before.read_bytes()).hexdigest(),'expected_failure_observed':True,'exit_code':result.returncode,'log':str(out/'before-conflict-ui.log'),'passed_checks':0,'reason':'Earlier production conflict list accepted an automatic toggle binding already assigned to StickyGuard.'}
    print('EXPECTED BASELINE FAILURE: automatic binding incorrectly accepted the existing StickyGuard key (not counted as passing checks).')
results['ui_native']=run('automatic-ui-native',prefix+ui_tests)
results['ui_unified']=run('automatic-ui-unified',prefix+ui_tests,['-DRA2_UNIFIED'])
results['dispatcher']=run('automatic-dispatcher',route)

# Evaluate actual category classification for every feature/key. The concrete
# creation scan also guards against losing the repair row or cloning its key.
layout=r'''
#include "automatic_features.h"
#include "../../suite/source/settings_categories.h"
#include <iostream>
#include <cstdlib>
using namespace assist;unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))std::abort();}while(0)
int main(){for(const auto&e:autofeatures::Entries){int ids[]={e.control,e.keyControl};int section=e.feature==autofeatures::MagnetronGlobal?1:e.feature==autofeatures::AutoCS?4:2;CHECK(suite::categories::row(section,ids,2)==suite::categories::Automatic);}
int repair[]={100,200},interval[]={312},autoBoard[]={111,211},repairRule[]={780};CHECK(suite::categories::row(0,repair,2)==suite::categories::Automatic);CHECK(suite::categories::row(2,interval,1)==suite::categories::Automatic);CHECK(suite::categories::row(0,autoBoard,2)==suite::categories::Control);CHECK(suite::categories::row(2,repairRule,1)==suite::categories::Rules);std::cout<<"PASS automatic category checks="<<checks<<"\n";}
'''
results['categories']=run('automatic-categories',layout)
tactical=(production/'tactical_controls.h').read_text()
auto_section=tactical.split('heading(2,902,',1)[1].split('heading(3,',1)[0]
assert tactical.count('repairCheck=')==1 and tactical.count('keys[0]=key(200,hotkeys[0])')==1
assert 'repairCheck=toggle(100' in auto_section and 'repairEdit=number(312,' in auto_section
assert '500–30000 · 默认 2000' in auto_section
assert 'for(int i:{11,1,2,3,4,5,6,7})' in tactical
assert auto_section.index('repairCheck=')<auto_section.index('repairEdit=')<auto_section.index('ejectCheck=')
# These controls are parameters; they have not silently become independent features.
catalog=(production/'automatic_features.h').read_text()
for id in (312,700,701,781,782,895,881,9497,9200):assert not re.search(r'\{\w+,'+str(id)+',',catalog)
results['layout_source_contract']={'passed':True,'checks':14,'notes':'Creation/adjacency/legacy-command and parameter exclusion source checks; no browser render.'}
paths=[Path(__file__),ui_fixture,hot_fixture]+[production/name for name in (
'automatic_features.h','automatic_hotkeys_host.h','additional_settings.h','main.cpp','tactical_controls.h','auto_settings_ui.h','type_bindings_ui.h','hotkey_host.h','hotkey_routes.h','extra_settings_io.h','settings_dependencies.h','hover_help.h')]+[root/'suite/source/settings_categories.h',root/'shared/source/key_gesture.h',root/'shared/source/feature_availability.h']
report={'schema':1,'scope':'automatic repair relocation/interval and per-feature toggle hotkeys','passed':True,'total_checks':sum(x['checks'] for x in results.values()),'results':results,'negative_reproduction':negative,'source_sha256':{str(p.relative_to(repo)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},'limitations':['Portable Linux execution with Win32 controls, keyboard, disk and shared-memory boundary substitutes.','Full shipped settings handler and hotkey dispatcher execute; relevant owner/registration/persistence/sync statements are extracted from production.','Not a Windows game, multiplayer or browser rendering test. Existing broad fixture test bodies are not executed.']}
(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print('PASS focused automatic controls checks='+str(report['total_checks']))
