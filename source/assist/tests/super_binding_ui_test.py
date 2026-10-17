"""Execute the shipped superweapon list refresh, including reorder/replacement."""
from pathlib import Path
import subprocess, tempfile, sys
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[2]
source=(root/'assist/source/main.cpp').read_text()
body=source[source.index('static std::vector<std::string> visibleSuperIDs;' if 'static std::vector<std::string> visibleSuperIDs;' in source else 'static void fillSW(){'):source.index('static void fillTech(')]
if 'selectedSuperID' not in body: body+='\nstatic std::string selectedSuperID(){return row>=0?snap.supers[row].id:std::string{};}\n'
prefix=r'''
#include <cassert>
#include <cstdint>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
using LPARAM=intptr_t;
constexpr int CB_GETCURSEL=1,CB_GETDROPPEDSTATE=2,CB_RESETCONTENT=3,CB_ADDSTRING=4,CB_SETCURSEL=5,HKM_SETHOTKEY=6;
static bool updatingControls=false;struct ControlUpdate{bool old=updatingControls;ControlUpdate(){updatingControls=true;}~ControlUpdate(){updatingControls=old;}};
static int swList=1,swHotkey=2,row=-1,key=0;static bool dropped=false;
static std::vector<std::wstring> rows;
static std::map<std::string,int> bindings={{"A",65},{"B",66},{"C",67}};
static struct{int swCount=2;struct SW{std::string id;bool ready=true;int frames=0;const wchar_t*name=L"SW";};SW supers[3]={{"A"},{"B"},{"C"}};}snap;
static std::wstring wide(const std::string&s){return {s.begin(),s.end()};}
static int binding(const std::string&s){return bindings[s];}
static std::wstring keyText(int k){return std::to_wstring(k);}
static int SendMessageW(int,int msg,int value,LPARAM data){switch(msg){
case CB_GETCURSEL:return row;case CB_GETDROPPEDSTATE:return dropped;case CB_RESETCONTENT:rows.clear();row=-1;return 0;
case CB_ADDSTRING:rows.emplace_back((const wchar_t*)data);return rows.size()-1;case CB_SETCURSEL:row=value;return 0;
case HKM_SETHOTKEY:assert(updatingControls);key=value;return 0;}return 0;}
'''
suffix=r'''
int main(){
 fillSW();assert(selectedSuperID()=="A"&&key==65&&!updatingControls);
 row=1;std::swap(snap.supers[0],snap.supers[1]);fillSW();assert(selectedSuperID()=="B"&&row==0&&key==66);
 snap.supers[0]=snap.supers[2];snap.swCount=1;fillSW();assert(selectedSuperID()=="C"&&key==67);
 dropped=true;snap.supers[0].id="A";fillSW();assert(selectedSuperID()=="C"); // visible row remains authoritative while dropdown open
 dropped=false;fillSW();assert(selectedSuperID()=="A"&&key==65);
 snap.swCount=0;fillSW();assert(selectedSuperID().empty()&&key==0);
}
'''
with tempfile.TemporaryDirectory() as temp:
    cpp=Path(temp)/'ui.cpp';binary=Path(temp)/'ui';cpp.write_text(prefix+body+suffix)
    subprocess.run(['g++','-std=c++17',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('PASS: superweapon UI retains identity, refreshes binding, clears removed entries and preserves dropdown identity')
