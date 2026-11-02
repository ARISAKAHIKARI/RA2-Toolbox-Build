// Production coordination header with controlled Win32, memory and gateway
// boundaries. It does not execute a game receiver or Windows message queue.
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
using HWND=void*;using DWORD=uint32_t;using UINT=unsigned;
constexpr UINT WM_NULL=0,WM_KEYDOWN=0x100,WM_KEYUP=0x101,WM_SYSKEYDOWN=0x104,WM_SYSKEYUP=0x105;
struct MSG{HWND hwnd=nullptr;UINT message=0;uintptr_t wParam=0;intptr_t lParam=0;};
static HWND window=reinterpret_cast<HWND>(1),foreground=window;
static uint32_t match=7,lastHouse=0x100000;static int moStage=1;static bool moHolding=false;
static bool held[256]{},postFails=false;static int commits=0;static std::vector<MSG> posted;
static std::vector<uint8_t> memory(0x1000000);
template<class T>static T get(uint32_t at){T v{};assert(at+sizeof v<=memory.size());std::memcpy(&v,memory.data()+at,sizeof v);return v;}
template<class T>static void put(uint32_t at,T v){assert(at+sizeof v<=memory.size());std::memcpy(memory.data()+at,&v,sizeof v);}
static bool read(uint32_t at,void*out,size_t size){if(at+size>memory.size())return false;std::memcpy(out,memory.data()+at,size);return true;}
static bool IsWindow(HWND hwnd){return hwnd==window;}
static HWND GetForegroundWindow(){return foreground;}
static DWORD GetCurrentProcessId(){return 42;}
static void GetWindowThreadProcessId(HWND hwnd,DWORD*out){*out=hwnd==window?42:1;}
static short GetKeyState(int vk){return held[vk]?short(0x8000):0;}
static bool PostMessageW(HWND hwnd,UINT message,uintptr_t wp,intptr_t lp){if(postFails)return false;posted.push_back({hwnd,message,wp,lp});return true;}
namespace vanilla{enum class Domain{NoOptions};namespace EventType{constexpr int PlanCommit=0x2B;}}
namespace intent{struct EventOrder{vanilla::Domain domain{};int player=0,eventKind=0;};}
static bool submitConstructed(const intent::EventOrder&event){assert(event.domain==vanilla::Domain::NoOptions&&event.eventKind==0x2B&&event.player==0);++commits;return true;}
#include "../source/mo_patrol_planning_bridge.h"
static MSG message(UINT kind,unsigned key,intptr_t lp=0){return {window,kind,key,lp};}
static void setup(uint16_t binding='P'|0x200){
 moPlanningUI=MoPlanningUI{};std::fill(memory.begin(),memory.end(),0);std::memset(held,0,sizeof held);commits=0;posted.clear();postFails=false;match=7;lastHouse=0x100000;foreground=window;moStage=1;moHolding=false;
 put(0xA83D4C,lastHouse);put(lastHouse+0x30,0);
 // Genuine nativekeys::load parses this bounded synthetic CommandClass table.
 put(0x87F680,0x200000u);put(0x87F684,1u);put(0x87F688,1u);
 put(0x200000,uint32_t(binding));put(0x200004,0x210000u);put(0x210000,0x220000u);put(0x220004,0x600000u);
 const uint8_t getName[]={0xB8,0,0,0x90,0,0xC3};std::memcpy(memory.data()+0x600000,getName,6);std::memcpy(memory.data()+0x900000,"PlanningMode",13);
}
int main(){
 MSG begin=message(WM_KEYDOWN,'K');
 // A custom-only patrol is independent of both P and the historical Z key.
 setup();assert(moPlanningCapture(&begin));assert(moPlanningFinish(lastHouse));assert(commits==1&&posted.empty());
 // Native Ctrl+P retains commit ownership. A prefix release cannot finish
 // before P's actual release, and replay does not construct another event.
 setup();held[17]=held['P']=true;put(0x87F7E8+0x11B3,true);assert(moPlanningCapture(&begin));
 held[17]=false;auto ctrl=message(WM_KEYUP,17);assert(moPlanningMessage(&ctrl)&&ctrl.message==WM_NULL);assert(!moPlanningFinish(lastHouse));
 auto unrelated=message(WM_KEYUP,'Z');assert(!moPlanningMessage(&unrelated));
 held['P']=false;auto p=message(WM_KEYUP,'P',123);assert(moPlanningMessage(&p));assert(moPlanningFinish(lastHouse));assert(!commits&&posted.size()==2&&posted.back().wParam=='P'&&posted.back().lParam==123);
 // Physical P re-press postpones an old deferred release; only the latest
 // actual release is posted when construction is ready.
 setup('P');held['P']=true;put(0x87F7E8+0x11B3,true);assert(moPlanningCapture(&begin));held['P']=false;p=message(WM_KEYUP,'P',1);assert(moPlanningMessage(&p));held['P']=true;p=message(WM_KEYDOWN,'P');assert(!moPlanningMessage(&p));assert(!moPlanningFinish(lastHouse));held['P']=false;p=message(WM_KEYUP,'P',2);assert(moPlanningMessage(&p));assert(moPlanningFinish(lastHouse));assert(posted.size()==1&&posted[0].lParam==2&&!commits);
 // Unknown active native editing fails closed; custom-only capture remains
 // available when no native editor is active.
 setup();put(0x87F680,0u);put(0x87F7E8+0x11B3,true);assert(!moPlanningCapture(&begin));put(0x87F7E8+0x11B3,false);assert(moPlanningCapture(&begin));assert(moPlanningFinish(lastHouse));assert(commits==1);
 // Releasing an old match never posts into the next match's native editor.
 setup('P');held['P']=true;put(0x87F7E8+0x11B3,true);assert(moPlanningCapture(&begin));held['P']=false;p=message(WM_KEYUP,'P');assert(moPlanningMessage(&p));++match;moPlanningReset();assert(posted.empty()&&!commits);
 // A failed PostMessage is retried without a duplicate direct PlanCommit.
 setup('P');held['P']=true;put(0x87F7E8+0x11B3,true);assert(moPlanningCapture(&begin));held['P']=false;p=message(WM_KEYUP,'P');assert(moPlanningMessage(&p));postFails=true;assert(!moPlanningFinish(lastHouse));postFails=false;assert(moPlanningFinish(lastHouse));assert(posted.size()==1&&!commits);
 // Native editing begun after custom capture is coordinated as well.
 setup('P');assert(moPlanningCapture(&begin));held['P']=true;p=message(WM_KEYDOWN,'P');assert(!moPlanningMessage(&p));put(0x87F7E8+0x11B3,true);held['P']=false;p=message(WM_KEYUP,'P');assert(moPlanningMessage(&p));assert(moPlanningFinish(lastHouse));assert(posted.size()==1&&!commits);
}
