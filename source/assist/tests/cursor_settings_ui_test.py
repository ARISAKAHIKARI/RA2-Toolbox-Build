"""Execute the actual cursor settings handler against portable Win32 controls."""
from pathlib import Path
import subprocess, tempfile, sys
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[2]
main=(root/'cursor/source/main.cpp').read_text()
header=root/'cursor/source/live_settings_ui.h'
body=header.read_text() if header.exists() else 'static void applyScale(){'+main.split('static void applyScale(){',1)[1].split('// Previews are rendered',1)[0]
prefix=r'''
#include <algorithm>
#include <cassert>
#include <map>
#include <string>
#include <stdexcept>
using HWND=int;
constexpr int BM_GETCHECK=1,BST_CHECKED=1,CB_GETCURSEL=2;
static int scaleBox=1,offsetXBox=2,offsetYBox=3,gameWBox=4,gameHBox=5,screenWBox=6,screenHBox=7,dpiBox=8,smoothMsBox=9,clickGapBox=10,dpiModeBox=11,clickModeBox=12,smoothBox=13,adaptiveBox=14,clickBox=15,autoBox=16,fitBox=17,settingsHint=18;
static int scalePercent=100,offsetX=0,offsetY=0,gameWOverride=0,gameHOverride=0,screenWOverride=0,screenHOverride=0,dpiOverride=0,dpiMode=0,smoothMs=60,clickMode=0,clickGap=0,lastFrame=0;
static size_t lastPhase=0;static bool clickHook=false,autoScale=true,aspectFit=true,smoothing=true,adaptive=true,clickEnabled=true,diskOK=true;
static unsigned writes=0,cancels=0;static int savedClick=-1;
static std::map<int,int> fields;
static std::wstring hint;
struct Reset {void reset(){}void resetObservation(){}};static Reset tween,timing,activeMotion;
static struct{void cancel(){++cancels;}}burst;
static int numeric(int id,int lo,int hi){auto p=fields.find(id);if(p==fields.end()||p->second<lo||p->second>hi)throw std::runtime_error("invalid");return p->second;}
static int SendMessageW(int id,int,int,int){return fields[id];}
static void textIfChanged(int,const std::wstring&s){hint=s;}
static std::wstring ini(){return L"settings.ini";}
static bool WritePrivateProfileStringW(const wchar_t* section,const wchar_t* key,const wchar_t* value,const wchar_t*){++writes;if(std::wstring(section)==L"Click"&&std::wstring(key)==L"Enabled")savedClick=std::stoi(value);return diskOK;}
'''
suffix=r'''
int main(){
 fields={{1,100},{2,0},{3,0},{4,0},{5,0},{6,0},{7,0},{8,0},{9,60},{10,0},{11,0},{12,0},{13,1},{14,1},{15,1},{16,1},{17,1}};
 fields.erase(scaleBox);fields[smoothBox]=0;applyScale();
 assert(!smoothing&&scalePercent==100&&writes>0); // no obsolete hook handle is required // unrelated invalid input must not block a switch
 fields[scaleBox]=125;fields[gameWBox]=1920;fields[gameHBox]=0;fields[clickBox]=0;applyScale();
 assert(scalePercent==125&&!clickEnabled&&gameWOverride==0&&gameHOverride==0&&cancels>0);
 fields[clickBox]=1;applyScale();assert(clickEnabled&&savedClick==-1); // Shift5 belongs to tactical assistant, cursor must not persist it
 fields[gameHBox]=1080;fields.erase(smoothMsBox);fields[clickGapBox]=25;applyScale();
 assert(gameWOverride==1920&&gameHOverride==1080&&smoothMs==60&&clickGap==25);
 fields[smoothMsBox]=60;diskOK=false;fields[offsetXBox]=3;applyScale();assert(hint.find(L"失败")!=std::wstring::npos);
 unsigned prior=writes;diskOK=true;applyScale();assert(writes>prior&&hint==L"设置已自动保存");
}
'''
with tempfile.TemporaryDirectory() as temp:
    cpp=Path(temp)/'ui.cpp';binary=Path(temp)/'ui';cpp.write_text(prefix+body+suffix)
    subprocess.run(['g++','-std=c++17',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('PASS: cursor settings: independent toggles, atomic dimension pairs, valid-field persistence and disk retry; cursor does not overwrite tactical Shift5 configuration')
