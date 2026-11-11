"""Execute current display controls, INI assignments and disabled sale entry."""
from pathlib import Path
import argparse,re,json,subprocess
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[2]
def block(text,start):
 i=text.index(start);b=text.index('{',i);depth=1;j=b+1
 while depth:
  if text[j]=='{':depth+=1
  elif text[j]=='}':depth-=1
  j+=1
 return text[i:j]
io=(root/'assist/source/extra_settings_io.h').read_text();ui=(root/'assist/source/auto_settings_ui.h').read_text();controls=(root/'assist/source/tactical_controls.h').read_text()
loads=''.join(re.search(r'extra\.'+f+r'=[^;]+;',io).group() for f in ['enemySpecialOnly','enemySpecialMode','sellEnemy'])
saves=''.join(re.search(r'write\(L"Display",L"'+f+r'",[^;]+;',io).group() for f in ['EnemySpecialOnly','EnemySpecialMode'])
owner=block(ui,'if(id==9764&&select)');sale=block(ui,'if((id==860||id==722)&&notification==BN_CLICKED)')
button=next(l for l in controls.splitlines() if 'extraControls[SellEnemyEnabled]=toggle' in l)
unit=(root/'unit_monitor/source/display_settings_ui.h').read_text().replace('#include "../../suite/source/video_settings_ui.h"','')
flags=block(io,'static uint32_t additionalFlags()')
prefix=r'''
#define RA2_UNIFIED
#include "display_settings.h"
#include "player_test_policy.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cwchar>
#include <map>
#include <string>
#include <iostream>
using namespace assist;using HWND=int;using WPARAM=uintptr_t;using LPARAM=intptr_t;using BOOL=int;
constexpr int TRUE=1,FALSE=0,BST_CHECKED=1,BST_UNCHECKED=0,BN_CLICKED=0,CBN_SELCHANGE=1,EN_CHANGE=2,EN_KILLFOCUS=3;
constexpr int CB_ADDSTRING=32,CB_GETCURSEL=33,CBS_DROPDOWNLIST=34,BM_SETCHECK=1,CB_SETCURSEL=2,BM_GETCHECK=4,EM_SETLIMITTEXT=5;
constexpr int BS_AUTOCHECKBOX=1,WS_TABSTOP=2,WS_BORDER=4,ES_NUMBER=8,ES_AUTOHSCROLL=16;
static int ui=7,requestedOption=-1,requestedValue=-1,changes=0;
static std::map<int,int>fields;static std::map<int,bool>available;static std::map<std::wstring,int>ini;
static int LOWORD(WPARAM v){return v&65535;}static int HIWORD(WPARAM v){return v>>16;}static WPARAM wp(int id,int message){return id|(message<<16);}
static int GetDlgItem(int,int id){return id;}static int GetFocus(){return -1;}
static int SendMessageW(int h,int message,WPARAM w,LPARAM){if(message==CB_GETCURSEL||message==BM_GETCHECK)return fields[h];if(message==CB_SETCURSEL||message==BM_SETCHECK)fields[h]=int(w);return 0;}
static void EnableWindow(int h,bool on){available[h]=on;}static bool IsWindowEnabled(int h){return available[h];}
static void SetWindowTextW(int,const wchar_t*){}static int GetDlgItemInt(int,int h,BOOL*ok,int){*ok=true;return fields[h];}
namespace modernui {void heading(int){}void tip(int,const wchar_t*){}}
namespace suite {enum class VideoGroup{Monitor};struct VideoSettings{void sync(bool){}template<class C>void create(C,VideoGroup,int,int,int){}bool command(WPARAM){return false;}};
static DisplayState state;bool readDisplaySettings(DisplayState&out){out=state;return true;}void requestDisplaySetting(int o,int v){requestedOption=o;requestedValue=v;}}
static int statsInformationFPS(){return 144;}
static struct {bool enemySpecialOnly=false,sellEnemy=false,controlAll=false;
 bool frozen=false,antiTheft=false,constructionQueue=false,nativeAirWaypoints=false,airWaypoints=false,experimentalReveal=false,enter=false,air=false,disk=false,sellDrained=false,sellVehicles=false;
 int enemySpecialMode=0,airPathKey=0;}extra;
static int techMode=0;enum {Frozen=1,UnsafeTech=2,AntiTheft=4,ReserveConstruction=8,NativeAirWaypoints=16,AirWaypoints=32,ExperimentalReveal=64,AdvancedEnter=128,AircraftSpread=256,DiskPrimary=512,SellDrained=1024,SellVehicles=2048,SellEnemy=4096};
static struct {void clear(){}}pointerMatches;
static void changedSetting(){++changes;}static void syncAdditional(){}
static int value(const wchar_t*,const wchar_t*k,int d){auto i=ini.find(k);return i==ini.end()?d:i->second;}
static void write(const wchar_t*,const wchar_t*k,int v){ini[k]=v;}
static int extraControls[1]{};constexpr int SellEnemyEnabled=0;
static int toggle(int id,const wchar_t*,bool on){fields[id]=on;available[id]=true;return id;}static void next(){}
'''
code=prefix+unit+f'\nvoid load(){{const bool sellOptIn=true;{loads}}}\nvoid save(){{{saves}}}\nbool owner(int id,bool select){{{owner}return false;}}\nbool sale(int id,int notification){{{sale}return false;}}\nvoid button(){{{button}}}\n'+flags+r'''
int main(){
 auto create=[](const wchar_t*,const wchar_t*,int,int,int,int,int,int id){available[id]=true;return id;};createUnitSettings(create);
 const int mode=600+suite::EnemySpecialMode;assert(!available[mode]);suite::state.values[suite::EnemyMarkers]=1;syncUnitSettings(true);assert(!available[mode]);
 suite::state.values[suite::EnemySpecialOnly]=1;suite::state.values[suite::EnemySpecialMode]=1;syncUnitSettings(true);assert(available[mode]&&fields[mode]==1);
 fields[mode]=0;assert(unitSettingCommand(wp(mode,CBN_SELCHANGE)));assert(requestedOption==suite::EnemySpecialMode&&requestedValue==0);
 fields[9764]=1;assert(owner(9764,true)&&extra.enemySpecialMode==1);extra.enemySpecialOnly=true;save();extra.enemySpecialMode=0;extra.enemySpecialOnly=false;load();assert(extra.enemySpecialMode==1&&extra.enemySpecialOnly);
 ini[L"EnemySpecialMode"]=9;load();assert(extra.enemySpecialMode==1);ini[L"EnemySpecialMode"]=-9;load();assert(extra.enemySpecialMode==0);
 suite::state.values[suite::EnemySpecialOnly]=0;syncUnitSettings();requestedOption=-1;unitSettingCommand(wp(mode,CBN_SELCHANGE));assert(requestedOption==-1);
 assert(!suite::validDisplayEdit(suite::EnemySpecialMode,-1)&&!suite::validDisplayEdit(suite::EnemySpecialMode,2));
 static_assert(!playertest::SellAvailable);ini[L"SellEnemyBuildings"]=1;load();assert(!extra.sellEnemy);button();assert(!available[722]&&!fields[722]);
 fields[722]=1;assert(sale(722,BN_CLICKED));assert(!extra.sellEnemy&&!fields[722]&&!available[722]);extra.sellEnemy=true;assert(!(additionalFlags()&SellEnemy));
 std::cout<<"PASS production choice control/owner/INI, disabled parent gating, invalid values, old sale config/click/shared flags blocked\n";
}
'''
cpp=a.out/'settings.cpp';cpp.write_text(code);exe=a.out/'settings'
cmd=['g++','-std=c++17','-O1','-I'+str(root/'suite/source'),'-I'+str(root/'assist/source'),str(cpp),'-o',str(exe)]
r=subprocess.run(cmd,capture_output=True,text=True);(a.out/'compile.log').write_text(r.stdout+r.stderr);r.check_returncode()
r=subprocess.run([str(exe)],capture_output=True,text=True);(a.out/'result.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');r.check_returncode()
(a.out/'result.json').write_text(json.dumps({'pass':True,'live_windows_ui_tested':False})+'\n')
