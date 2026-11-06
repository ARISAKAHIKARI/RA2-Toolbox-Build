"""Run shipped spider-defense controls, independent participant profiles and Shared publication."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def block(text, marker):
    start = text.index(marker)
    cursor = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[cursor] == "{") - (text[cursor] == "}")
        cursor += 1
    return text[start:cursor]


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--out", type=Path, required=True)
args = parser.parse_args()
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=True)
source = Path(__file__).resolve().parents[2]
src = source / "assist/source"
ui = (src / "auto_settings_ui.h").read_text()
io = (src / "extra_settings_io.h").read_text()
bindings = (src / "type_bindings_ui.h").read_text()
controls = (src / "tactical_controls.h").read_text()
handler = block(ui, "static bool autoSettingChanged(int id,int notification)")
prologue = handler.split("{", 1)[1].split(" if(id==", 1)[0]
toggle = block(handler, "if(id==9496&&notification==BN_CLICKED)")
numeric = handler[handler.index(" auto number="):handler.index(" if(id==714&&edit)")]
participant_handler = block(handler, "if(id>=9200&&id<9200+2*featuretypes::Count)")
ore_toggle = block(handler, "if((id==781||id==782)&&notification==BN_CLICKED)")
ore_editor = block(handler, "if((id==783||id==784)&&edit)")
type_load = block(io, "for(int i=0;i<featuretypes::Count;++i){auto&f=featureTypes.filters[i]")
type_save = block(io, "for(int i=0;i<featuretypes::Count;++i){std::wstring custom=")
type_sync = "\n".join(block(bindings, "if(memcmp(&shared->" + name + ",")
                      for name in ("featureConfig", "oreConfig"))
changed = block(ui, "static void changedSetting()")
assert controls.count('toggle(9200+f*2,L"自定义点杀单位类型"') == 1
assert 'f==featuretypes::BurstRefresh||f==featuretypes::SpiderDefense' in controls
assert '矿车类型沿用上方' not in controls
load = "\n".join(re.search(r"extra\." + field + r"=[^;]+;", io)[0]
                 for field in ("minerDefenseEnabled", "minerDefenseRangeCells"))
save = "\n".join(re.findall(r'write\(L"MinerDefense",[^;]+;', io))
sync = "\n".join(re.findall(r"shared->minerDefense(?:RangeCells|Enabled)=[^;]+;", io))
assert len(re.findall(r'write\(L"MinerDefense",', save)) == 2
assert len(re.findall(r"shared->minerDefense", sync)) == 2

cpp = r'''
#include "runtime_feature_demand.h"
#include "single_player_fps.h"
#include "settings_dependencies.h"
#include "../../suite/source/settings_categories.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cwchar>
#include <map>
#include <string>
using namespace assist;
using BOOL=int;
constexpr int FALSE=0,EN_CHANGE=1,CBN_SELCHANGE=2,BN_CLICKED=3,BM_GETCHECK=4,BST_CHECKED=1;
static AdditionalSettings extra;
static Shared wire,*shared=&wire;
static featuretypes::Config featureTypes;
static oreconfig::Config oreTypes;
static std::wstring featureTypeText[featuretypes::Count],oreMiners,oreRefineries,editText;
static std::map<std::pair<std::wstring,std::wstring>,int> ini;
static std::map<int,int> fields;
static std::map<std::pair<std::wstring,std::wstring>,std::wstring> textIni;
static bool saveWriteOK=true;
static std::wstring path(const wchar_t*s){return s;}
static void GetPrivateProfileStringW(const wchar_t*section,const wchar_t*key,const wchar_t*fallback,wchar_t*out,int n,const wchar_t*){
 auto it=textIni.find({section,key});const auto text=it==textIni.end()?std::wstring(fallback):it->second;
 std::wcsncpy(out,text.c_str(),n-1);out[n-1]=0;
}
static int WritePrivateProfileStringW(const wchar_t*section,const wchar_t*key,const wchar_t*text,const wchar_t*){textIni[{section,key}]=text;return 1;}
static void GetWindowTextW(int,wchar_t*out,int n){std::wcsncpy(out,editText.c_str(),n-1);out[n-1]=0;}
static void MemoryBarrier(){}
static long InterlockedIncrement(volatile long*p){return ++*p;}
static bool ready=true,updatingControls=false;
static int ui=1,settingsRevision=0,paintRevision=0,syncs=0,wakes=0;
static int&changes=settingsRevision;
static bool autoSavePending=false;
static constexpr unsigned AutoSaveTimer=3901;
static void SetTimer(int,unsigned id,int milliseconds,void*){assert(id==AutoSaveTimer&&milliseconds==300);}
static std::wstring status;
static int minimapX=0,minimapY=0,minimapScale=0,minimapSecondaryX=0,minimapSecondaryY=0,minimapSecondaryWidth=0;
static int fps=0,topMargin=0,labelScalePercent=0,constructionScalePercent=0,shiftClickGap=0,advancedRangeCells=0;
static int advancedScanHz=0,repairMs=0,doubleMs=0,toastMs=0;
static struct{void clear(){assert(false);}}typeTap;
static void syncInformationRate(bool){assert(false);}
static bool hardwareBudgetAvailable(uint32_t){assert(false);return false;}
static std::wstring hardwareBudgetReason(uint32_t){assert(false);return L"";}
static int value(const wchar_t*s,const wchar_t*k,int fallback){auto i=ini.find({s,k});return i==ini.end()?fallback:i->second;}
static void write(const wchar_t*s,const wchar_t*k,int v){ini[{s,k}]=v;}
static void load(){LOAD TYPE_LOAD}
static void save(){SAVE TYPE_SAVE}
static void syncTypeConfig(){if(!shared)return;TYPE_SYNC}
CHANGED
static void syncAdditional(){SYNC ++syncs;}
static void wakeGame(){++wakes;}
static void setStatus(const std::wstring&text){status=text;}
static int GetDlgItem(int,int id){return id;}
static int SendMessageW(int id,int message,int,int){assert(message==BM_GETCHECK);return fields[id];}
static unsigned GetDlgItemInt(int,int id,BOOL*ok,int){*ok=fields.count(id)!=0;return *ok?unsigned(fields[id]):0;}
static bool route(int id,int notification){PROLOGUE TOGGLE PARTICIPANTS ORE_TOGGLE ORE_EDITOR NUMERIC return false;}
int main(){unsigned checks=0;auto check=[&](bool okay){assert(okay);++checks;};
 for(auto&key:extra.missionModeKeys)key=0;
 check(!extra.minerDefenseEnabled&&extra.minerDefenseRangeCells==1);
 load();syncAdditional();check(!wire.minerDefenseEnabled&&wire.minerDefenseRangeCells==1);
 check(!runtimeFeatureDemand(0,extra,false,false));
 check(!route(9496,EN_CHANGE)&&!route(9497,BN_CLICKED));
 for(int on:{1,0,1,0}){
  fields[9496]=on;int oldChanges=changes,oldSyncs=syncs,oldWakes=wakes;
  check(route(9496,BN_CLICKED));
  check(extra.minerDefenseEnabled==bool(on)&&wire.minerDefenseEnabled==unsigned(on));
  check(changes==oldChanges+1&&syncs==oldSyncs+1&&wakes==oldWakes+1);
  check(runtimeFeatureDemand(0,extra,false,false)==bool(on));
  save();extra.minerDefenseEnabled=!on;load();check(extra.minerDefenseEnabled==bool(on));
 }
 for(int n:{0,1,4,30}){
  fields[9497]=n;int oldChanges=changes,oldSyncs=syncs,oldWakes=wakes;
  check(route(9497,EN_CHANGE));check(extra.minerDefenseRangeCells==n&&wire.minerDefenseRangeCells==unsigned(n));
  check(changes==oldChanges+1&&syncs==oldSyncs+1&&wakes==oldWakes+1);
  check(!runtimeFeatureDemand(0,extra,false,false)); // Editing N does not enable the feature.
  save();extra.minerDefenseRangeCells=17;load();check(extra.minerDefenseRangeCells==n);
 }
 for(int invalid:{-1,31,65536}){
  fields[9497]=invalid;int oldChanges=changes,oldSyncs=syncs,oldWakes=wakes;
  check(route(9497,EN_CHANGE));check(extra.minerDefenseRangeCells==30&&wire.minerDefenseRangeCells==30);
  check(changes==oldChanges&&syncs==oldSyncs&&wakes==oldWakes&&!status.empty());
 }
 fields.erase(9497);int before=changes;check(route(9497,EN_CHANGE)&&changes==before&&extra.minerDefenseRangeCells==30);
 for(int saved:{-100,0,1,30,100}){
  ini[{L"MinerDefense",L"ExtraRangeCells"}]=saved;load();syncAdditional();
  check(extra.minerDefenseRangeCells==std::clamp(saved,0,30)&&wire.minerDefenseRangeCells==unsigned(std::clamp(saved,0,30)));
 }
 ini.clear();load();syncAdditional();check(!extra.minerDefenseEnabled&&extra.minerDefenseRangeCells==1);
 ini[{L"Modes",L"AutoOreReturn"}]=0;ini[{L"MinerDefense",L"Enabled"}]=1;
 load();syncAdditional();check(extra.minerDefenseEnabled&&wire.minerDefenseEnabled&&runtimeFeatureDemand(0,extra,false,false));
 for(int blocked:{0,1}){
  ready=blocked!=0;updatingControls=blocked!=0;int oldChanges=changes;
  fields[9496]=0;fields[9497]=10;check(!route(9496,BN_CLICKED)&&!route(9497,EN_CHANGE));
  check(changes==oldChanges&&extra.minerDefenseEnabled&&extra.minerDefenseRangeCells==1);
 }
 ready=true;updatingControls=false;
 const int feature=featuretypes::SpiderDefense,checkbox=9200+2*feature,editor=checkbox+1;
 auto&participants=featureTypes.filters[feature];
 check(feature==20&&featuretypes::Count==21);
 check(std::wstring(featuretypes::key(feature))==L"SpiderDefense");
 check(std::wstring(featuretypes::initial(feature))==L"HARV,CMIN");
 check(!participants.custom&&participants.count==2&&oreconfig::equal(participants.ids[0],"HARV")&&oreconfig::equal(participants.ids[1],"CMIN"));
 check(!settingsdeps::decision(editor,{}).enabled()&&settingsdeps::decision(checkbox,{}).enabled());
 // Exercise the actual type editor and publication independently from ore settings.
 oreTypes.customMiners=1;check(oreconfig::parse(L"UNRELATED_MINER",oreTypes.miners,oreTypes.minerCount));syncTypeConfig();
 const auto originalOre=oreTypes;const auto other=featureTypes.filters[featuretypes::Dodge];
 fields[checkbox]=1;long seq=wire.featureConfigSeq;int changedBefore=changes;
 check(route(checkbox,BN_CLICKED));check(participants.custom&&wire.featureConfig.filters[feature].custom);
 check(changes==changedBefore+1&&wire.featureConfigSeq==seq+2&&!(wire.featureConfigSeq&1));
 check(autoSavePending);
 for(const auto&sample:{std::pair<std::wstring,int>{L"HARV,HTNK",2},{L"E1,MOD_TANK",2},{L"harv，e1;E1\nMOD-TANK",3}}){
  editText=sample.first;seq=wire.featureConfigSeq;changedBefore=changes;
  check(route(editor,EN_CHANGE));check(changes==changedBefore+1&&participants.count==sample.second);
  check(featureTypeText[feature]==sample.first&&wire.featureConfigSeq==seq+2&&!(wire.featureConfigSeq&1));
  check(!std::memcmp(&participants,&wire.featureConfig.filters[feature],sizeof participants));
  check(!std::memcmp(&oreTypes,&originalOre,sizeof oreTypes)&&!std::memcmp(&other,&featureTypes.filters[featuretypes::Dodge],sizeof other));
  save();check(ini[{L"FeatureTypes",L"SpiderDefenseCustom"}]==1&&textIni[{L"FeatureTypes",L"SpiderDefense"}]==sample.first);
  participants={};featureTypeText[feature]=L"";load();syncTypeConfig();
  check(participants.custom&&participants.count==sample.second&&featureTypeText[feature]==sample.first);
  check(!std::memcmp(&participants,&wire.featureConfig.filters[feature],sizeof participants));
 }
 check(oreconfig::equal(participants.ids[0],"HARV")&&oreconfig::equal(participants.ids[1],"E1")&&oreconfig::equal(participants.ids[2],"MOD-TANK"));
 // The editor accepts the documented 23-character/512-type boundaries and
 // rejects the first overflow without disturbing the accepted configuration.
 editText=std::wstring(23,L'A');check(route(editor,EN_CHANGE));check(participants.count==1&&std::strlen(participants.ids[0])==23);
 std::wstring fullList;for(int n=0;n<oreconfig::Limit;++n){if(n)fullList+=L",";fullList+=L"TYPE_"+std::to_wstring(n);}
 editText=fullList;check(route(editor,EN_CHANGE));check(participants.count==oreconfig::Limit);
 check(!std::memcmp(&participants,&wire.featureConfig.filters[feature],sizeof participants));
 save();participants={};load();syncTypeConfig();check(participants.count==oreconfig::Limit&&featureTypeText[feature]==fullList);
 // Invalid edits preserve the last accepted profile and the published snapshot.
 for(const auto&invalid:{std::wstring(L"HTNK,$E1"),std::wstring(24,L'A'),std::wstring(L"矿车"),fullList+L",OVERFLOW"}){
  const auto prior=participants;const auto priorText=featureTypeText[feature];seq=wire.featureConfigSeq;changedBefore=changes;status.clear();editText=invalid;
  check(route(editor,EN_CHANGE));check(changes==changedBefore&&wire.featureConfigSeq==seq&&!status.empty());
  check(!std::memcmp(&prior,&participants,sizeof prior)&&featureTypeText[feature]==priorText);
 }
 // An explicitly empty custom list selects nobody and persists that intent.
 editText=L"";check(route(editor,EN_CHANGE));check(participants.custom&&participants.count==0);
 check(!featuretypes::allowed(featureTypes,feature,"HARV")&&!featuretypes::allowed(featureTypes,feature,"HTNK"));
 save();participants={};load();syncTypeConfig();
 check(participants.custom&&participants.count==0&&wire.featureConfig.filters[feature].count==0);
 // Turning the custom list off preserves its saved entries without enabling defense.
 editText=L"HARV,E1";check(route(editor,EN_CHANGE));fields[9496]=0;check(route(9496,BN_CLICKED));
 fields[checkbox]=0;check(route(checkbox,BN_CLICKED));check(!participants.custom&&participants.count==2&&!extra.minerDefenseEnabled);
 check(!wire.featureConfig.filters[feature].custom&&wire.featureConfig.filters[feature].count==2&&!wire.minerDefenseEnabled);
 save();participants={};load();syncTypeConfig();check(!participants.custom&&participants.count==2&&featureTypeText[feature]==L"HARV,E1");
 // Editing the separate ore-return list cannot rewrite spider participants.
 const auto spiderBefore=participants;const auto spiderTextBefore=featureTypeText[feature];
 editText=L"OTHER_MINER";check(route(783,EN_CHANGE));fields[781]=0;check(route(781,BN_CLICKED));
 check(!oreTypes.customMiners&&oreconfig::equal(oreTypes.miners[0],"OTHER_MINER"));
 check(!std::memcmp(&spiderBefore,&participants,sizeof participants)&&featureTypeText[feature]==spiderTextBefore);
 check(!std::memcmp(&participants,&wire.featureConfig.filters[feature],sizeof participants));
 // Existing pre.2 profiles with no new keys retain default miner behavior.
 ini.erase({L"FeatureTypes",L"SpiderDefenseCustom"});textIni.erase({L"FeatureTypes",L"SpiderDefense"});
 load();syncTypeConfig();check(!participants.custom&&participants.count==2&&featureTypeText[feature]==L"HARV,CMIN");
 for(int id:{9496,9497,9498,checkbox,editor})check(suite::categories::row(2,&id,1)==suite::categories::Automatic);
 check(Version==99);std::printf("PASS miner defense settings checks=%u\n",checks);
}
'''
for marker, text in sorted({"LOAD": load, "SAVE": save, "SYNC": sync, "PROLOGUE": prologue,
                     "TOGGLE": toggle, "NUMERIC": numeric, "PARTICIPANTS": participant_handler,
                     "ORE_TOGGLE": ore_toggle, "ORE_EDITOR": ore_editor,
                     "TYPE_LOAD": type_load, "TYPE_SAVE": type_save, "TYPE_SYNC": type_sync,
                     "CHANGED": changed}.items(), key=lambda pair: -len(pair[0])):
    cpp = cpp.replace(marker, text)
cpp_file = out / "miner_defense_settings.cpp"
cpp_file.write_text(cpp)
results = {}
for mode, defines in (("standalone", []), ("unified", ["-DRA2_UNIFIED"])):
    exe = out / ("settings-" + mode)
    subprocess.run(["g++", "-std=c++17", "-O2", "-UNDEBUG", *defines,
                    "-I" + str(src), str(cpp_file), "-o", str(exe)], check=True)
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    print(result.stdout, end="")
    assert result.returncode == 0, result.stdout + result.stderr
    results[mode] = result.stdout.strip()
files = ["additional_settings.h", "extra_settings_io.h", "auto_settings_ui.h",
         "protocol.h", "runtime_feature_demand.h", "feature_type_config.h", "ore_type_config.h",
         "type_bindings_ui.h", "tactical_controls.h", "settings_dependencies.h"]
(out / "result.json").write_text(json.dumps({
    "passed": True, "scope": __doc__, "results": results,
    "source_sha256": {name: hashlib.sha256((src / name).read_bytes()).hexdigest() for name in files},
    "real_windows_ui": False,
}, indent=2) + "\n")
