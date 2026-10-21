"""Exercise production suite display messages with portable window stubs.
Checks ownership, bounds, coherent cross-thread state and control notifications.
Not a Windows UI/compositor test.
"""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
suite=(root/'suite/source/main.cpp').read_text()
bus=' void publishDisplaySettings'+suite.split(' void publishDisplaySettings',1)[1].split('\n}\nstatic void save()',1)[0]
assist=(root/'assist/source/main.cpp').read_text()
forward=assist.split(' if(msg==suite::ApplyDisplaySetting){',1)[1].split('\n if(msg==suite::ModuleEnable)',1)[0]
forward=forward.rsplit('}',1)[0]
prefix=r'''
#include "display_settings.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <cassert>
#include <map>
#include <set>
#include <string>
#include <iostream>
using HWND=int;using WPARAM=uintptr_t;using LPARAM=intptr_t;
constexpr int TRUE=1,BST_CHECKED=1,BST_UNCHECKED=0,BN_CLICKED=0,CBN_SELCHANGE=1,EN_CHANGE=2;
constexpr int BM_SETCHECK=1,CB_SETCURSEL=2,WM_COMMAND=3;
namespace suite {constexpr int ApplyDisplaySetting=83,DisplaySettingsChanged=84;}
static std::atomic<HWND> displayPanes[3]{};
static std::mutex displayMutex;static suite::DisplayState displayState;static bool displayReady=false;
static std::atomic<int> posted{0};
static bool PostMessageW(HWND h,int msg,WPARAM,LPARAM){assert(h);assert(msg==83||msg==84);++posted;return true;}
static bool ready=true,updatingControls=false;static int ui=7,notifications=0,lastID=0,lastNotification=0;
static std::map<int,int> fields;
struct ControlUpdate {ControlUpdate(){assert(!updatingControls);updatingControls=true;}~ControlUpdate(){updatingControls=false;}};
static int GetDlgItem(int,int id){return id;}
static WPARAM MAKEWPARAM(int id,int notification){return id|(notification<<16);}
static void SendMessageW(int h,int message,WPARAM wp,LPARAM lp){
 if(message==WM_COMMAND){assert(!updatingControls&&h==ui);++notifications;lastID=wp&65535;lastNotification=wp>>16;assert(lp==lastID);}
 else {assert(updatingControls);fields[h]=int(wp);}
}
static void SetWindowTextW(int h,const wchar_t* text){assert(updatingControls);fields[h]=std::stoi(text);}
static void publishSuiteDisplay(){}
'''
suffix=r'''
int main(){
 using namespace suite;DisplayState out;assert(!readDisplaySettings(out));
 assert(!requestDisplaySetting(RefreshMode,1));displayPanes[0]=10;displayPanes[1]=11;displayPanes[2]=ui;
 std::set<int> ids;
 for(int key=0;key<DisplayOptionCount;++key){const auto&s=displaySpecs[key];assert(ids.insert(s.id).second);
  for(int value:{s.min,s.max}){assert(requestDisplaySetting(key,value));int before=notifications;forwardDisplay(ui,key,value);assert(notifications==before+1&&lastID==s.id&&fields[s.id]==value);
   assert(lastNotification==(s.control==DisplayControl::Check?BN_CLICKED:s.control==DisplayControl::Choice?CBN_SELCHANGE:EN_CHANGE));}
  for(int bad:{s.min-1,s.max+1}){assert(!requestDisplaySetting(key,bad));int before=notifications;forwardDisplay(ui,key,bad);assert(notifications==before);}
 }
 for(int key:{-1,int(DisplayOptionCount)})assert(!requestDisplaySetting(key,0));
 ready=false;int before=notifications;forwardDisplay(ui,Capacity,1);assert(notifications==before);ready=true;
 std::atomic<bool> done{false};std::thread writer([&]{for(int n=1;n<=10000;++n){DisplayState s;s.values.fill(n);s.rateMilli=n;s.gapUs=n;publishDisplaySettings(s);}done=true;});
 do{if(readDisplaySettings(out)){for(auto v:out.values)assert(v==out.rateMilli);assert(out.gapUs==out.rateMilli);}}while(!done);
 writer.join();assert(readDisplaySettings(out)&&out.rateMilli==10000);int count=posted;publishDisplaySettings(out);assert(posted==count);
 std::cout<<"PASS 11 routed setting IDs: valid endpoints, invalid edits, owner-thread notifications and not-ready suppression\n";
 std::cout<<"PASS 10000 cross-thread state publications: coherent snapshots and unchanged-state notification coalescing\n";
}
'''
with tempfile.TemporaryDirectory(prefix='suite-display-') as temp:
    cpp=Path(temp)/'test.cpp';exe=Path(temp)/'test'
    cpp.write_text(prefix+'\nnamespace suite {\n'+bus+'\n}\nstatic int forwardDisplay(int w,WPARAM wp,LPARAM lp){'+forward+'}\n'+suffix)
    subprocess.run(['g++','-std=c++17','-O2','-pthread','-I'+str(root/'suite/source'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
