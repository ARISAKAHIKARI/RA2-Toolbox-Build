"""Run the shipped miner-defense UI, persistence and Shared publication paths."""
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
handler = block(ui, "static bool autoSettingChanged(int id,int notification)")
prologue = handler.split("{", 1)[1].split(" if(id==", 1)[0]
toggle = block(handler, "if(id==9496&&notification==BN_CLICKED)")
numeric = handler[handler.index(" auto number="):handler.index(" if(id==714&&edit)")]
load = "\n".join(re.search(r"extra\." + field + r"=[^;]+;", io)[0]
                 for field in ("minerDefenseEnabled", "minerDefenseRangeCells"))
save = "\n".join(re.findall(r'write\(L"MinerDefense",[^;]+;', io))
sync = "\n".join(re.findall(r"shared->minerDefense(?:RangeCells|Enabled)=[^;]+;", io))
assert len(re.findall(r'write\(L"MinerDefense",', save)) == 2
assert len(re.findall(r"shared->minerDefense", sync)) == 2

cpp = r'''
#include "runtime_feature_demand.h"
#include "single_player_fps.h"
#include "../../suite/source/settings_categories.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
using namespace assist;
using BOOL=int;
constexpr int FALSE=0,EN_CHANGE=1,CBN_SELCHANGE=2,BN_CLICKED=3,BM_GETCHECK=4,BST_CHECKED=1;
static AdditionalSettings extra;
static Shared wire,*shared=&wire;
static std::map<std::pair<std::wstring,std::wstring>,int> ini;
static std::map<int,int> fields;
static bool ready=true,updatingControls=false;
static int ui=1,changes=0,syncs=0,wakes=0;
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
static void load(){LOAD}
static void save(){SAVE}
static void changedSetting(){++changes;}
static void syncAdditional(){SYNC ++syncs;}
static void wakeGame(){++wakes;}
static void setStatus(const std::wstring&text){status=text;}
static int GetDlgItem(int,int id){return id;}
static int SendMessageW(int id,int message,int,int){assert(id==9496&&message==BM_GETCHECK);return fields[id];}
static unsigned GetDlgItemInt(int,int id,BOOL*ok,int){*ok=fields.count(id)!=0;return *ok?unsigned(fields[id]):0;}
static bool route(int id,int notification){PROLOGUE TOGGLE NUMERIC return false;}
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
 for(int id:{9496,9497,9498})check(suite::categories::row(2,&id,1)==suite::categories::Automatic);
 check(Version==98);std::printf("PASS miner defense settings checks=%u\n",checks);
}
'''
for marker, text in {"LOAD": load, "SAVE": save, "SYNC": sync, "PROLOGUE": prologue,
                     "TOGGLE": toggle, "NUMERIC": numeric}.items():
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
         "protocol.h", "runtime_feature_demand.h"]
(out / "result.json").write_text(json.dumps({
    "passed": True, "scope": __doc__, "results": results,
    "source_sha256": {name: hashlib.sha256((src / name).read_bytes()).hexdigest() for name in files},
    "real_windows_ui": False,
}, indent=2) + "\n")
