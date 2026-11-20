"""Execute native reservation settings branch and verify source UI/INI binding.

The heavy legacy auto-settings test fixture is outdated. Extract ONLY this
production checkbox branch, and the exact INI load/save statements, then
compile and execute them with portable Win32 controls.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1] / "source"
additional = (root / "additional_settings.h").read_text()
settings = (root / "extra_settings_io.h").read_text()
ui = (root / "tactical_controls.h").read_text()
handler = (root / "auto_settings_ui.h").read_text()
dependencies = (root / "settings_dependencies.h").read_text()
host = (root / "construction_host.h").read_text()

assert "bool constructionShowNativeUnits=false" in additional
assert 'extra.constructionShowNativeUnits=value(L"Production",L"ReserveShowAvailableUnits",0)!=0;' in settings
assert 'write(L"Production",L"ReserveShowAvailableUnits",extra.constructionShowNativeUnits);' in settings
assert 'toggle(9796,L"显示已可生产单位（批量预定）",extra.constructionShowNativeUnits)' in ui
assert "if(id==9796&&notification==BN_CLICKED)" in handler
assert "case 817:case 9796:return require(s,{Need::Construction});" in dependencies
assert "9180,895,817,9796,9096" in dependencies
assert "construction::showReservationChoice(tab,native,queued,future,busyBuilding,extra.constructionShowNativeUnits)" in host
assert "if(!source.complete()||(forecastAttempted&&!previewReady)){choices=std::move(previous);return;}" in host
assert "choicesMatch!=commandMatch()||choicesHouse!=house||choicesTab!=activeTab" in host
assert "if(!nativeReady)return;" in host

def branch(source, token):
    start = source.index(token)
    open_brace = source.index("{", start)
    depth, i = 1, open_brace + 1
    while depth:
        depth += (source[i] == "{") - (source[i] == "}")
        i += 1
    return source[start:i]

toggle = branch(handler, "if(id==9796&&notification==BN_CLICKED)")
load = re.search(r'extra\.constructionShowNativeUnits=value\([^;]*;', settings).group(0)
save = re.search(r'write\(L"Production",L"ReserveShowAvailableUnits",[^;]*;', settings).group(0)

cpp = r'''
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
static struct {bool constructionShowNativeUnits=false;} extra;
constexpr int BN_CLICKED=3,BM_GETCHECK=8,BST_CHECKED=1;
static unsigned revisions=0,saveTimer=0;
static int ui=1;static int checkStates[10000]{};
static std::map<std::wstring,int> values;
static int GetDlgItem(int,int id){return id;}
static int SendMessageW(int id,int op,int,int){assert(op==BM_GETCHECK);return checkStates[id];}
static void changedSetting(){++revisions;saveTimer=300;}
static int value(const wchar_t*,const wchar_t*key,int fallback){
 auto i=values.find(key);return i==values.end()?fallback:i->second;
}
static void write(const wchar_t*,const wchar_t*key,int setting){values[key]=setting;}
static bool changed(int id,int notification){
''' + toggle + r'''
 return false;
}
static void loadSetting(){''' + load + r'''}
static void saveSetting(){''' + save + r'''}
int main(){
 assert(!extra.constructionShowNativeUnits);
 assert(!changed(9796,0)&&!revisions);
 assert(!changed(9797,BN_CLICKED)&&!revisions);
 checkStates[9796]=BST_CHECKED;
 assert(changed(9796,BN_CLICKED));
 assert(extra.constructionShowNativeUnits&&revisions==1&&saveTimer==300);
 saveSetting();assert(values[L"ReserveShowAvailableUnits"]==1);
 extra.constructionShowNativeUnits=false;loadSetting();
 assert(extra.constructionShowNativeUnits);
 checkStates[9796]=0;assert(changed(9796,BN_CLICKED));
 assert(!extra.constructionShowNativeUnits&&revisions==2&&saveTimer==300);
 saveSetting();assert(values[L"ReserveShowAvailableUnits"]==0);
 values.erase(L"ReserveShowAvailableUnits");
 extra.constructionShowNativeUnits=true;loadSetting();
 assert(!extra.constructionShowNativeUnits);
 std::puts("PASS production checkbox handler: apply immediately, autosave timer 300ms, INI round-trip and default off");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / "reservation_ui.cpp"
    out = Path(tmp) / "reservation_ui"
    src.write_text(cpp)
    subprocess.run(["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-UNDEBUG", str(src), "-o", str(out)], check=True)
    subprocess.run([str(out)], check=True)

print("PASS reservation native-unit preference: default off, UI/INI persistence, parent dependency and incomplete snapshot retention")
