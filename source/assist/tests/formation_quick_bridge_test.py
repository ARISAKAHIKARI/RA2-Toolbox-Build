"""Execute quick-placement production bridge with controlled Win32/world boundaries.
Checks real continuation/commit code; does not prove game pathfinding or multiplayer.
"""
from pathlib import Path
import json
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
fixture=r'''
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
#include "mo_orders.h"
using namespace assist;
using UINT=unsigned;using DWORD=uint32_t;using HWND=uintptr_t;using WPARAM=uintptr_t;using LPARAM=intptr_t;
constexpr UINT WM_NULL=0,WM_KEYDOWN=0x100,WM_KEYUP=0x101,WM_SYSKEYDOWN=0x104,WM_SYSKEYUP=0x105;
constexpr UINT WM_MOUSEMOVE=0x200,WM_LBUTTONDOWN=0x201,WM_LBUTTONUP=0x202,WM_RBUTTONDOWN=0x204,WM_RBUTTONUP=0x205,WM_MBUTTONDOWN=0x207,WM_MBUTTONUP=0x208;
constexpr int VK_MBUTTON=4,VK_SHIFT=16,VK_CONTROL=17,VK_MENU=18,VK_ESCAPE=27;
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
 volatile long formationSeq=0,formationPolicySeq=0,moGesturePolicySeq=0;
 uint32_t moFeatures=0;moorders::GesturePolicy moGestures;
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
 msg.lParam=(message>=WM_MOUSEMOVE&&message<=0x20D)?MAKELPARAM(short(x),short(y)):(repeat?LPARAM(1u<<30):0);
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
 setup();message(WM_MBUTTONDOWN);auto before=sh->formationPreview;
 assert(before.active&&before.shape==formation::quick::Shape&&before.count==25);
 message(WM_MBUTTONUP);assert(!sh->formationPreview.active&&formationMemory.quick&&formationMemory.count==25);
 auto quota=[](const formation::Slot*slots,int n){std::map<std::pair<int,int>,int> cells;for(int i=0;i<n;++i){assert(slots[i].valid);assert(++cells[std::make_pair(slots[i].x/256,slots[i].y/256)]<=3);}return cells;};
 auto original=quota(before.slots,before.count);assert(original.size()==9);
 formationTick(lastHouse,snap.frame,clockValue);assert(commands.size()==25);
 formationFollow.cell=40040;formationFollow.frame=snap.frame;formationFollow.tick=clockValue;
 ++snap.frame;put(0xA8ED84,snap.frame);clockValue+=30;shared.heartbeat=clockValue;commands.clear();
 formationContinue(clockValue);
 assert(formationJobCount==25&&formationMemory.quick);
 auto moved=quota(sh->formationPreview.slots,sh->formationPreview.count);assert(moved.size()==original.size());
 formationTick(lastHouse,snap.frame,clockValue);assert(commands.size()==25);
 std::map<uint32_t,int> actual;for(auto command:commands)++actual[command.cell];
 assert(actual.size()==9);for(auto cell:actual)assert(cell.second<=3);
 // Adding a chord cancels the quick capture and its release cannot commit.
 setup();message(WM_MBUTTONDOWN);assert(formationQuick);held[VK_CONTROL]=true;
 assert(!dispatch(WM_KEYDOWN,VK_CONTROL));assert(!formationQuick&&!sh->formationPreview.active);
 message(WM_MBUTTONUP);assert(!formationJobCount&&commands.empty());
 // An explicit click at another release cell places at that cell rather than
 // introducing a two-node drag when no drag was actually observed.
 setup();message(WM_MBUTTONDOWN);message(WM_MBUTTONUP,0,false,130,130);
 assert(formationMemory.quick&&formationJobCount==25);int same=0;
 for(int i=0;i<formationJobCount;++i)if(formationJobs[i].goal.x/256==31&&formationJobs[i].goal.y/256==31)++same;
 assert(same==3);
 // Rebind both the held keyboard key and the mouse, including an X-button.
 setup();sh->moGestures.keys[3]='G'|0x3000;sh->moGestures.buttons[3]=2;held['G']=true;
 message(0x20B,1u<<16);assert(formationQuick&&formationQuickMouse==2);message(0x20C,1u<<16);
 assert(formationMemory.quick&&formationJobCount==25);
 setup();sh->moGestures.keys[3]='G';sh->moGestures.buttons[3]=4;held['G']=true;
 message(WM_RBUTTONDOWN);assert(formationQuick);assert(!dispatch(WM_LBUTTONDOWN));assert(!formationQuick&&!sh->formationPreview.active);
 message(WM_RBUTTONUP);assert(!formationJobCount);
 // Rebinding the active policy cancels the pending click instead of sending
 // the old destinations with a newly chosen mouse/keyboard authorization.
 setup();message(WM_MBUTTONDOWN);sh->moGestures.buttons[3]=2;message(WM_MBUTTONUP);assert(!formationJobCount);
 std::cout<<"PASS production quick placement: quotas survive commit and remembered-formation translation; chord cancellation and click release destination\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp=Path(tmp)/'quick_bridge.cpp'
    executable=Path(tmp)/'quick_bridge'
    cpp.write_text(fixture.replace('@LAYOUT_HEADER@',json.dumps(str(root/'assist/source/formation.h'))))
    subprocess.run(['g++','-std=c++17','-O2','-I'+str(root/'assist/source'),str(cpp),'-o',str(executable)],check=True)
    subprocess.run([str(executable)],check=True,timeout=20)
