"""Exercise production aircraft settings snippets without legacy UI fixture dependencies.

The full Win32 Host/Runtime is separately checked by paired Windows CI; this
fixture tests extracted production expressions and their validation side effects.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1] / "source"
io = (ROOT / "extra_settings_io.h").read_text()
handler = (ROOT / "auto_settings_ui.h").read_text()
controls = (ROOT / "tactical_controls.h").read_text()
defaults = (Path(__file__).resolve().parents[2] / "defaults" / "Assist.ini").read_text()
shared_defaults = (Path(__file__).resolve().parents[2] / "shared" / "source" / "initial_profiles.h").read_text()


def statement(text: str, start: str) -> str:
    assert text.count(start) == 1, start
    return start + text.split(start, 1)[1].split(";", 1)[0] + ";"


radius_load = statement(io, "extra.aircraftDamageRadiusCells=")
percent_load = statement(io, "extra.aircraftDamageCoveragePercent=")
radius_save = statement(io, 'write(L"Cooperative",L"AirDamageRadiusCells",')
percent_save = statement(io, 'write(L"Cooperative",L"AirDamagePercent",')
radius_sync = statement(io, "shared->aircraftDamageRadiusCells=")
percent_sync = statement(io, "shared->aircraftDamageCoveragePercent=")
begin = "if((id==9794||id==9795)&&edit){"
end = "if(id==9195&&select){"
assert handler.count(begin) == 1 and handler.count(end) == 1
edit_handler = handler.split(begin, 1)[1].split(end, 1)[0]
edit_handler = begin + edit_handler
assert "changedSetting();" in edit_handler and "syncAdditional();" in edit_handler
for key, value in (("AirDamageRadiusCells", "5"), ("AirDamagePercent", "120")):
    assert key + "=" + value in defaults and key + "=" + value in shared_defaults
assert 'number(9794,L"范围战车' in controls and 'number(9795,L"范围战车' in controls

code = r"""
#include "cooperative_types.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <map>
#include <string>
using namespace assist;
using BOOL=int;
constexpr int FALSE=0, EN_CHANGE=1;
static bool ready=true,updatingControls=false;
static int ui=1;
static unsigned revision=0,updates=0;
static std::wstring status;
struct Values {
 int aircraftDamageRadiusCells=cooperative::DefaultDamageRadiusCells;
 int aircraftDamageCoveragePercent=cooperative::DefaultDamagePercent;
} extra;
struct Runtime {
 uint32_t aircraftDamageRadiusCells=5,aircraftDamageCoveragePercent=120;
} backing,*shared=&backing;
static std::map<std::wstring,int> file,edits;
static int value(const wchar_t*,const wchar_t*k,int fallback){
 auto it=file.find(k);return it==file.end()?fallback:it->second;
}
static void write(const wchar_t*,const wchar_t*k,int v){file[k]=v;}
static int GetDlgItemInt(int,int id,BOOL*valid,int){
 const auto it=edits.find(std::to_wstring(id));
 *valid=(it!=edits.end());return *valid?it->second:0;
}
static void setStatus(const std::wstring&s){status=s;}
static void changedSetting(){++revision;}
static void syncAdditional(){++updates;}
static void load(){
""" + radius_load + "\n" + percent_load + r"""
}
static void save(){
""" + radius_save + "\n" + percent_save + r"""
}
static void publish(){
""" + radius_sync + "\n" + percent_sync + r"""
}
static bool changed(int id,int notification){
 if(!ready||updatingControls)return false;
 const bool edit=notification==EN_CHANGE;
""" + edit_handler + r"""
 return false;
}
int main(){
 load();
 assert(extra.aircraftDamageRadiusCells==5&&extra.aircraftDamageCoveragePercent==120);
 assert(!changed(9794,0));
 edits[L"9794"]=7;
 assert(changed(9794,EN_CHANGE)&&extra.aircraftDamageRadiusCells==7&&revision==1&&updates==1);
 assert(changed(9794,EN_CHANGE)&&revision==1&&updates==1);
 edits[L"9795"]=175;
 assert(changed(9795,EN_CHANGE)&&extra.aircraftDamageCoveragePercent==175&&revision==2&&updates==2);
 publish();assert(shared->aircraftDamageRadiusCells==7&&shared->aircraftDamageCoveragePercent==175);
 save();extra={};load();
 assert(extra.aircraftDamageRadiusCells==7&&extra.aircraftDamageCoveragePercent==175);
 for(const auto&bad: {std::pair<int,int>{9794,0},{9794,65},{9795,0},{9795,501}}){
  edits[std::to_wstring(bad.first)]=bad.second;
  assert(changed(bad.first,EN_CHANGE)&&revision==2&&updates==2);
 }
 file[L"AirDamageRadiusCells"]=-7;file[L"AirDamagePercent"]=999;
 load();assert(extra.aircraftDamageRadiusCells==1&&extra.aircraftDamageCoveragePercent==500);
 file.clear();load();assert(extra.aircraftDamageRadiusCells==5&&extra.aircraftDamageCoveragePercent==120);
 // Existing profiles with no new keys retain legacy behavior.
 cooperative::Request request{};
 assert(cooperative::radiusCells(request.airRadiusCells)==5);
 assert(cooperative::damagePercent(request.airDamagePercent)==120);
 request.airRadiusCells=7;request.airDamagePercent=175;
 assert(cooperative::validRange(request.airRadiusCells,request.airDamagePercent));
 return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="ra2_air_settings_") as temp:
    src = Path(temp) / "air_settings.cpp"
    binary = Path(temp) / "air_settings"
    src.write_text(code)
    subprocess.run(
        ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra",
         "-Wno-unused-parameter", "-UNDEBUG", "-fsanitize=undefined",
         "-fno-sanitize-recover=all", "-I", str(ROOT), str(src), "-o", str(binary)],
        check=True)
    subprocess.run([str(binary)], check=True)
print("PASS aircraft production settings: 5/120 defaults, 7/175 live edit, persistence, runtime publication, invalid range isolation and clamp")
