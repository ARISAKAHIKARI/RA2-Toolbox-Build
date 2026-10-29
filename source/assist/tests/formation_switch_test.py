"""Execute the production formation bridge with controlled Win32/world boundaries.

The actual input handler, layout, publication, assignment, commit and job-drain
code is included unchanged. Native queries and nativeMission are boundary
doubles: this does not execute the final DLL, native event constructors, game
pathfinding, Windows rendering or multiplayer delivery.

--layout-header allows the same regression to be run against a previous layout
header, without replacing or rewriting the production bridge under test.
"""
from pathlib import Path
import argparse
import json
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--layout-header', type=Path, default=root / 'assist/source/formation.h')
args = parser.parse_args()

fixture = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include @LAYOUT_HEADER@
using namespace assist;
using UINT=unsigned;using DWORD=uint32_t;using HWND=uintptr_t;using WPARAM=uintptr_t;using LPARAM=intptr_t;
constexpr UINT WM_NULL=0,WM_KEYDOWN=0x100,WM_KEYUP=0x101,WM_SYSKEYDOWN=0x104,WM_SYSKEYUP=0x105;
constexpr UINT WM_MOUSEMOVE=0x200,WM_LBUTTONDOWN=0x201,WM_LBUTTONUP=0x202,WM_RBUTTONDOWN=0x204,WM_RBUTTONUP=0x205;
constexpr int VK_SHIFT=16,VK_CONTROL=17,VK_MENU=18,VK_ESCAPE=27;
static uint16_t LOWORD(LPARAM v){return uint16_t(v);}static uint16_t HIWORD(LPARAM v){return uint16_t(uintptr_t(v)>>16);}
static LPARAM MAKELPARAM(short x,short y){return uint16_t(x)|(uint32_t(uint16_t(y))<<16);}
struct MSG{HWND hwnd=1;UINT message=0;WPARAM wParam=0;LPARAM lParam=0;};
struct POINT{int x=100,y=100;};struct RECT{int left=0,top=0,right=800,bottom=600;};
static std::array<bool,256> held{};static POINT cursor;
static bool GetClientRect(HWND,RECT*r){*r={};return true;}static bool GetCursorPos(POINT*p){*p=cursor;return true;}
static bool ScreenToClient(HWND,POINT*){return true;}static short GetKeyState(int k){return held[k]?short(0x8000):0;}
static HWND GetForegroundWindow(){return 1;}static DWORD GetCurrentProcessId(){return 42;}
static void GetWindowThreadProcessId(HWND,DWORD*p){*p=42;}
static void MemoryBarrier(){}static void InterlockedIncrement(volatile long*p){++*p;}
static std::vector<uint8_t> memory(0x1000000);
template<class T>static T get(uint32_t p){assert(size_t(p)+sizeof(T)<=memory.size());T value{};std::memcpy(&value,memory.data()+p,sizeof value);return value;}
template<class T>static void put(uint32_t p,T value){assert(size_t(p)+sizeof(T)<=memory.size());std::memcpy(memory.data()+p,&value,sizeof value);}
static bool read(uint32_t p,void*out,size_t n){if(size_t(p)+n>memory.size())return false;std::memcpy(out,memory.data()+p,n);return true;}
static std::vector<uint32_t> actors,selected;
struct Vec{int count=0;};static Vec vec(uint32_t p){assert(p==0xA8ECB8);return {int(selected.size())};}
static uint32_t item(Vec,int n){return selected.at(n);}
static struct{bool contains(uint32_t p)const{return std::find(actors.begin(),actors.end(),p)!=actors.end();}}worldObjects;
static uint32_t lastHouse=0x100000,match=7;static bool fault=false;
static int kind(uint32_t p){return worldObjects.contains(p)?1:0;}
static bool own(uint32_t p,uint32_t h){return get<uint32_t>(p+0x21C)==h;}
static bool live(uint32_t p){return get<int>(p+0x6C)>0;}
static uint32_t typeOf(uint32_t p,int){return get<uint32_t>(p+0x6C4);}
static bool GuardedObjectAir(uint32_t,int*air){*air=0;return true;}
static int selections=0;static void indexWorld(int){++selections;}
static uint32_t worldByID(uint32_t id){for(auto p:actors)if(get<uint32_t>(p+0x10)==id)return p;return 0;}
static struct{std::map<uint32_t,uint32_t> tokens;uint32_t get(uint32_t id){return tokens[id];}}manualVersions;
static void cancelNative(uint32_t p){++manualVersions.tokens[get<uint32_t>(p+0x10)];}
static struct{int status=1,commandStatus=1,frame=100,session=0,chat=0;}snap;
static uint32_t clockValue=1000;static int notifications=0,formationJobCount=0;
static struct Shared{
 bool stop=false,foreground=true;uint32_t heartbeat=1000;
 volatile long formationSeq=0,formationPolicySeq=0;
 formation::Policy formationPolicy;formation::Preview formationPreview;
 int formationJobs=0,formationAccepted=0,formationRejected=0;
 uint32_t formationConsumedUntil=0,formationKey=0,formationKeyAck=0,formationKeyCode=0;
}shared;
static Shared*sh=&shared;static uint32_t sampleClock(Shared*){return clockValue;}static void notifyHost(){++notifications;}
namespace gameentry{enum class Client{Assist};static bool active(Client){return true;}}
namespace eventdelivery{static bool mayQueue(int mode){return mode==0||mode==3;}}
namespace external{namespace display{struct State{bool visible()const{return true;}};template<class Read>static State inspect(Read){return {};}}}
namespace pointerinput{enum{AirWaypoint=1};struct Intent{int width=0,height=0,cameraX=0,cameraY=0,operation=0,x=0,y=0;};}
static uint32_t pointerCell=0,pointerTarget=0,pointerTargetID=0;static int pointerGroundZ=0,locationQueries=0;
static bool pointerLocation(const pointerinput::Intent&p){++locationQueries;int x=30+(p.x-100)/30,y=30+(p.y-100)/30;pointerCell=y*1000+x;pointerTarget=pointerTargetID=0;pointerGroundZ=0;return true;}
static std::set<std::pair<int,int>> blocked;static int cellQueries=0,heightQueries=0;
static uint32_t readMapCell(int x,int y){++cellQueries;if(x<10||x>=61||y<10||y>=61||blocked.count({x,y}))return 0;return 0x300000+uint32_t((y-10)*51+x-10)*0x200;}
template<class R,class...Args>static R call(uint32_t at,uint32_t self,Args...){assert(at==0x578080&&self==0x87F7E8);++heightQueries;return R(0);}
static keygesture::Tracker runtimeKeys;
namespace taskarbiter{enum class Owner{Formation};}
struct Command{uint32_t id=0,cell=0;};static std::vector<Command> commands;static int feedbacks=0;
static bool simulationReady(){return true;}static int outgoingRoom(){return 256-int(commands.size());}
static bool nativeMission(uint32_t house,uint32_t id,int mission,uint32_t target,uint32_t cell,int targetKind,int extra1,int extra2,taskarbiter::Owner owner){
 assert(house==lastHouse&&mission==2&&!target&&targetKind==11&&!extra1&&!extra2&&owner==taskarbiter::Owner::Formation);
 commands.push_back({id,cell});return true;
}
static void inputFeedback(uint32_t,int,bool){++feedbacks;}
#include "formation_bridge.h"

static constexpr uint32_t shapeKeys[6]={'Z','X','C','V','B','N'};
static void setup(int button=2){
 std::fill(memory.begin(),memory.end(),0);actors.clear();selected.clear();blocked.clear();commands.clear();manualVersions.tokens.clear();held.fill(false);runtimeKeys={};
 shared={};clockValue=1000;snap={};snap.status=snap.commandStatus=1;snap.frame=100;cursor={100,100};fault=false;sh=&shared;
 sh->formationPolicy.enabled=1;sh->formationPolicy.mouse=button;sh->formationPolicy.hold=VK_MENU;
 std::copy(std::begin(shapeKeys),std::end(shapeKeys),sh->formationPolicy.keys);
 put(0xA8E9A0,true);put(0xA83D4C,lastHouse);put(0xA8ED84,snap.frame);put(0xA8B238,snap.session);
 put(0x887324,0x200000u);put(0x8A00A4,800);put(0x8A00A8,600);
 for(int n=0;n<25;++n){uint32_t p=0x10000+n*0x1000;actors.push_back(p);put(p+0x10,uint32_t(100+n));put(p+0x21C,lastHouse);put(p+0x6C4,0x190000u);put(p+0x6C,100);put(p+0x9C,(18+n%5)*256+128);put(p+0xA0,(18+n/5)*256+128);}
 selected=actors;resetFormation();formationEpoch=match;selections=notifications=cellQueries=heightQueries=locationQueries=feedbacks=0;
}
static bool dispatch(UINT message,uint32_t key=0,bool repeat=false,int x=100,int y=100){
 MSG msg;msg.message=message;msg.wParam=key;
 msg.lParam=(message>=WM_MOUSEMOVE&&message<=WM_RBUTTONUP)?MAKELPARAM(short(x),short(y)):(repeat?LPARAM(1u<<30):0);
 if(message==WM_KEYDOWN||message==WM_SYSKEYDOWN||message==WM_KEYUP||message==WM_SYSKEYUP){
  const bool down=message==WM_KEYDOWN||message==WM_SYSKEYDOWN;held[key]=down;
  uint8_t keys[256]{};for(unsigned i=0;i<256;++i)keys[i]=held[i];
  runtimeKeys.edge(key,down,repeat,clockValue,keygesture::chord(key,keys));
 }
 bool consumed=formationMessage(&msg);if(consumed)assert(msg.message==WM_NULL);
 assert((sh->formationSeq&1)==0);return consumed;
}
static void message(UINT m,uint32_t key=0,bool repeat=false,int x=100,int y=100){assert(dispatch(m,key,repeat,x,y));}
static void activate(){held[VK_MENU]=true;message(WM_SYSKEYDOWN,VK_MENU);assert(sh->formationPreview.active&&sh->formationPreview.count==25&&selections==1);}
using Geometry=std::set<std::pair<int,int>>;
static Geometry geometry(const formation::Preview&p){Geometry out;for(int n=0;n<p.count;++n){assert(p.slots[n].valid);out.insert({p.slots[n].x,p.slots[n].y});}assert(int(out.size())==p.count);return out;}
static bool sameSlots(const formation::Preview&a,const formation::Preview&b){return a.count==b.count&&std::memcmp(a.slots,b.slots,sizeof a.slots)==0;}
static std::map<uint32_t,uint32_t> destinations(const formation::Preview&p){std::map<uint32_t,uint32_t> out;for(int n=0;n<p.count;++n)out[p.units[n].id]=p.slots[n].y/256*1000+p.slots[n].x/256;return out;}
static void checkJobs(const formation::Preview&preview){
 assert(formationJobCount==preview.count&&sh->formationAccepted==preview.count&&!sh->formationRejected);
 for(int n=0;n<formationJobCount;++n){const auto&job=formationJobs[n];int i=0;while(i<preview.count&&preview.units[i].id!=job.unit.id)++i;assert(i<preview.count);const auto&s=preview.slots[i];assert(job.goal.x==s.x&&job.goal.y==s.y&&job.goal.z==s.z&&job.goal.valid==s.valid);}
}
static void drainExact(const formation::Preview&preview){
 assert(!formationDirected);for(int n=0;n<formationJobCount;++n)assert(!formationJobs[n].approach);
 formationTick(lastHouse,snap.frame,clockValue);assert(commands.size()==size_t(preview.count)&&feedbacks==1);
 std::map<uint32_t,uint32_t> actual;for(auto c:commands){assert(!actual.count(c.id));actual[c.id]=c.cell;}
 assert(actual==destinations(preview));
}
int main(){
 int cases=0;
 // No WM_MOUSEMOVE or clock advancement occurs between distinct shape keys.
 setup();activate();std::set<Geometry> shapes;
 for(int shape=0;shape<6;++shape){auto before=sh->formationPreview;int samples=selections;message(WM_SYSKEYDOWN,shapeKeys[shape]);auto p=sh->formationPreview;
  assert(p.shape==shape&&p.revision>before.revision&&p.tick==1000&&!p.dragging&&selections==samples);
  assert(shapes.insert(geometry(p)).second&&"stationary shape selection must change actual destination geometry");
  int reads=cellQueries;message(WM_SYSKEYDOWN,shapeKeys[shape],true);assert(sameSlots(p,sh->formationPreview)&&cellQueries==reads);
  message(WM_SYSKEYUP,shapeKeys[shape]);
 }assert(clockValue==1000);++cases;
 // A shared key cycles once per physical press, with the same stationary cursor.
 setup();std::fill(std::begin(sh->formationPolicy.keys),std::end(sh->formationPolicy.keys),'Z');activate();Geometry prior=geometry(sh->formationPreview);
 for(int expected:{1,2,3,4,5,0}){message(WM_SYSKEYDOWN,'Z');auto p=sh->formationPreview;assert(p.shape==expected&&geometry(p)!=prior);prior=geometry(p);
  int reads=cellQueries;message(WM_SYSKEYDOWN,'Z',true);assert(sh->formationPreview.shape==expected&&sameSlots(p,sh->formationPreview)&&cellQueries==reads);message(WM_SYSKEYUP,'Z');
 }assert(clockValue==1000);++cases;
 // Exact per-ID commits after switching, including moved units and small jitter.
 for(int button:{1,2})for(int shape=0;shape<6;++shape){
  setup(button);activate();uint32_t previous=shapeKeys[(shape+5)%6];message(WM_SYSKEYDOWN,previous);message(WM_SYSKEYUP,previous);auto original=geometry(sh->formationPreview);
  message(WM_SYSKEYDOWN,shapeKeys[shape]);message(WM_SYSKEYUP,shapeKeys[shape]);auto p=sh->formationPreview;
  assert(p.shape==shape&&p.count==25&&geometry(p)!=original);
  int reads=cellQueries,heights=heightQueries,samples=selections;
  for(int n=0;n<25;++n){put(actors[n]+0x9C,(45-n%5)*256+128);put(actors[n]+0xA0,(45-n/5)*256+128);}
  UINT down=button==1?WM_LBUTTONDOWN:WM_RBUTTONDOWN,up=button==1?WM_LBUTTONUP:WM_RBUTTONUP;
  message(down);assert(sameSlots(p,sh->formationPreview));clockValue+=20;message(WM_MOUSEMOVE,0,false,103,102);assert(sameSlots(p,sh->formationPreview));
  message(up,0,false,101,100);assert(!sh->formationPreview.active&&!sh->formationPreview.dragging&&sameSlots(p,sh->formationPreview));
  assert(cellQueries==reads&&heightQueries==heights&&selections==samples&&"click must not solve or resample the selected units");checkJobs(p);drainExact(p);++cases;
 }
 // The committed preview includes the actual obstacle/rounding resolution.
 setup();blocked.insert({30,30});activate();message(WM_SYSKEYDOWN,'C');message(WM_SYSKEYUP,'C');auto obstructed=sh->formationPreview;
 assert(!geometry(obstructed).count({30*256+128,30*256+128}));message(WM_RBUTTONDOWN);message(WM_RBUTTONUP);checkJobs(obstructed);drainExact(obstructed);++cases;
 // Release must keep the last dragged layout even when release coordinates differ.
 for(int button:{1,2}){
  setup(button);activate();message(WM_SYSKEYDOWN,'X');message(WM_SYSKEYUP,'X');UINT down=button==1?WM_LBUTTONDOWN:WM_RBUTTONDOWN,up=button==1?WM_LBUTTONUP:WM_RBUTTONUP;
  message(down);clockValue+=20;message(WM_MOUSEMOVE,0,false,180,135);auto p=sh->formationPreview;assert(p.dragging);int heights=heightQueries;
  message(up,0,false,200,150);assert(sameSlots(p,sh->formationPreview)&&heightQueries==heights);checkJobs(p);++cases;
  setup(button);activate();message(WM_SYSKEYDOWN,'C');message(WM_SYSKEYUP,'C');message(down);selected.pop_back();message(up);
  assert(!sh->formationPreview.active&&!formationJobCount&&commands.empty());++cases;
 }
 // Explicit Press and EXT preserve the same group as old bindings. Unsupported
 // Release/Hold shapes stay native and never silently act as presses.
 setup();sh->formationPolicy.keys[1]='X'|0x1000;sh->formationPolicy.keys[2]='C'|0x1800;sh->formationPolicy.keys[3]='V'|0x2000;sh->formationPolicy.keys[4]='B'|0x3000;activate();
 message(WM_SYSKEYDOWN,'X');assert(sh->formationPreview.shape==1);message(WM_SYSKEYUP,'X');
 message(WM_SYSKEYDOWN,'C');assert(sh->formationPreview.shape==2);message(WM_SYSKEYUP,'C');
 assert(!dispatch(WM_SYSKEYDOWN,'V')&&!dispatch(WM_SYSKEYUP,'V')&&sh->formationPreview.shape==2);
 assert(!dispatch(WM_SYSKEYDOWN,'B')&&!dispatch(WM_SYSKEYUP,'B')&&sh->formationPreview.shape==2);++cases;
 // Double Press selects only on the second physical down, not its release.
 setup();sh->formationPolicy.keys[1]='X'|0x9000;activate();message(WM_SYSKEYDOWN,'X');assert(sh->formationPreview.shape==0);message(WM_SYSKEYUP,'X');clockValue+=100;message(WM_SYSKEYDOWN,'X');assert(sh->formationPreview.shape==1);message(WM_SYSKEYUP,'X');assert(sh->formationPreview.shape==1);++cases;
 // Releasing a chord prefix ends a held preview and cannot be resurrected by
 // putting Ctrl back while G is still down or by the OS key-repeat stream.
 setup();sh->formationPolicy.hold='G'|0x3200;assert(!dispatch(WM_KEYDOWN,VK_CONTROL));message(WM_KEYDOWN,'G');assert(formationHolding&&sh->formationPreview.active);
 assert(!dispatch(WM_KEYUP,VK_CONTROL));assert(!formationHolding&&!sh->formationPreview.active);
 assert(!dispatch(WM_KEYDOWN,VK_CONTROL));assert(!dispatch(WM_KEYDOWN,'G',true));assert(!sh->formationPreview.active);
 message(WM_KEYUP,'G');message(WM_KEYDOWN,'G');assert(sh->formationPreview.active);++cases;
 // Tick-side validation also handles a missing modifier-up notification and
 // changing the configured hold while a prior preview is active.
 held[VK_CONTROL]=false;formationTick(lastHouse,snap.frame,clockValue);assert(!formationHolding&&!sh->formationPreview.active);++cases;
 setup();sh->formationPolicy.hold='G'|0x3200;assert(!dispatch(WM_KEYDOWN,VK_CONTROL));message(WM_KEYDOWN,'G');sh->formationPolicy.hold='H'|0x3200;
 assert(!dispatch(WM_MOUSEMOVE));assert(!formationHolding&&!sh->formationPreview.active);++cases;
 // A started drag keeps its frozen preview through keyboard release and the
 // subsequent pointer-up submits exactly those slots, preserving prior input.
 setup();sh->formationPolicy.hold='G'|0x3200;assert(!dispatch(WM_KEYDOWN,VK_CONTROL));message(WM_KEYDOWN,'G');message(WM_RBUTTONDOWN);clockValue+=20;message(WM_MOUSEMOVE,0,false,180,135);auto drag=sh->formationPreview;assert(drag.dragging);
 assert(!dispatch(WM_KEYUP,VK_CONTROL));assert(!formationHolding&&formationButton&&sh->formationPreview.active);message(WM_RBUTTONUP);assert(sameSlots(drag,sh->formationPreview));checkJobs(drag);++cases;
 // Double Alt remains a latched mode independent of whether Alt is held.
 setup();activate();message(WM_SYSKEYUP,VK_MENU);clockValue+=100;message(WM_SYSKEYDOWN,VK_MENU);assert(formationModal&&sh->formationPreview.active);message(WM_SYSKEYUP,VK_MENU);assert(formationModal&&sh->formationPreview.active);++cases;
 // A double-hold enters on the second key-down and ends when its prefix goes.
 setup();sh->formationPolicy.hold='G'|0xb200;assert(!dispatch(WM_KEYDOWN,VK_CONTROL));assert(!dispatch(WM_KEYDOWN,'G'));assert(!sh->formationPreview.active);assert(!dispatch(WM_KEYUP,'G'));clockValue+=100;message(WM_KEYDOWN,'G');assert(formationHolding&&sh->formationPreview.active);
 assert(!dispatch(WM_KEYUP,VK_CONTROL));assert(!formationHolding&&!sh->formationPreview.active);++cases;
 std::cout<<"PASS "<<cases<<" production bridge cases: stationary six-shape switch, same-clock cycling, repeat suppression, exact per-ID preview/job/nativeMission destinations for both buttons, moved units and jitter without recomputation, obstacle resolution, release freeze and membership cancellation, Press/EXT compatibility, unsupported modes, chord cancellation/repeat/rebind, drag ownership, modal Alt and double-hold\n";
}
'''

with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / 'formation_switch.cpp'
    executable = Path(tmp) / 'formation_switch'
    cpp.write_text(fixture.replace('@LAYOUT_HEADER@', json.dumps(str(args.layout_header.resolve()))))
    subprocess.run(['g++', '-std=c++17', '-O2', '-I' + str(root / 'assist/source'),
                    str(cpp), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True, timeout=20)

# Execute the host's actual bridge-routing function as well. The synthetic
# window/mailbox boundary deliberately makes no real Windows input claim.
host_function = (root / 'assist/source/formation_host.h').read_text()
host_function = host_function[host_function.index('static bool formationHotkey'):]
host_fixture = r'''
#include <cassert>
#include <cstdint>
#include <iostream>
#include "formation.h"
#include "type_hotkey.h"
using namespace assist;
using WORD=uint16_t;using LONG=int32_t;
constexpr unsigned VK_SHIFT=16,VK_CONTROL=17,VK_MENU=18;
static uint32_t now=1000,scene=1,settingsRevision=1;
static typekeys::Tap formationTap;
static uint32_t GetTickCount(){return now;}static uint32_t commandMatch(){return scene;}
static void MemoryBarrier(){}static void InterlockedIncrement(volatile LONG*p){++*p;}
static int wakes=0;static void wakeGame(){++wakes;}
static struct{bool formationEnabled=true;uint32_t formationHold=18,formationKeys[6]{};}extra;
static struct Shared{formation::Preview formationPreview;uint32_t formationKey=0,formationKeyAck=0,formationKeyCode=0;}state,*shared=&state;
'''+host_function+r'''
int main(){
 state.formationPreview.active=1;extra.formationKeys[0]='X'|0x1000;
 assert(formationHotkey('X'|0x400)&&wakes==1&&state.formationKeyCode=='X');state.formationKeyAck=state.formationKey;
 assert(formationHotkey('X'|0x1800)&&wakes==2&&state.formationKeyCode=='X');state.formationKeyAck=state.formationKey;
 extra.formationHold=17|0x3000;assert(formationHotkey('X'|0x200)&&wakes==3);state.formationKeyAck=state.formationKey;
 extra.formationKeys[0]='X'|0x9000;++scene;
 assert(formationHotkey('X')&&wakes==3);now+=100;assert(formationHotkey('X')&&wakes==4&&state.formationKeyCode==('X'|0x8000));state.formationKeyAck=state.formationKey;
 extra.formationKeys[0]='X'|0x2000;assert(!formationHotkey('X')&&wakes==4);
 extra.formationKeys[0]='X'|0x3000;assert(!formationHotkey('X')&&wakes==4);
 extra.formationKeys[0]='X';state.formationPreview.active=0;assert(!formationHotkey('X'));
 state.formationPreview.active=1;extra.formationEnabled=false;assert(!formationHotkey('X'));
 // A first tap cannot carry through preview closure or settings changes.
 extra.formationEnabled=true;extra.formationKeys[0]='X'|0x9000;++settingsRevision;
 assert(formationHotkey('X')&&wakes==4);state.formationPreview.active=0;assert(!formationHotkey('X'));
 state.formationPreview.active=1;now+=50;assert(formationHotkey('X')&&wakes==4);
 ++settingsRevision;now+=50;assert(formationHotkey('X')&&wakes==4);
 now+=50;assert(formationHotkey('X')&&wakes==5);state.formationKeyAck=state.formationKey;
 assert(formationHotkey('X')&&wakes==5);assert(!formationHotkey('Y'));
 now+=50;assert(formationHotkey('X')&&wakes==5);
 std::cout<<"PASS production host shape routing: Press/EXT normalization, configured modifier, second-down Double Press, unsupported modes, inactive/disabled gates\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp=Path(tmp)/'formation_host_keys.cpp';exe=Path(tmp)/'formation_host_keys'
    cpp.write_text(host_fixture)
    subprocess.run(['g++','-std=c++17','-O2','-I'+str(root/'assist/source'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=20)
