#include <windows.h>
#include <gdiplus.h>
#include <cassert>
#include <cstdio>
#include <map>
#include <tuple>
#include <vector>
#include "../../shared/source/overlay_stack.h"
#include "../../shared/source/overlay_visibility.h"
// The runner exposes private members in a generated copy of the production
// header. Only window/input APIs are mocked; Panel::proc/update are unchanged.
static HWND gameWindow=reinterpret_cast<HWND>(uintptr_t(0x1111)),panelWindow=reinterpret_cast<HWND>(uintptr_t(0x2222)),captureWindow=nullptr;
static DWORD tick=1000;static void*panelPointer=nullptr;static void(*captureChanged)()=nullptr;static int invalidations=0,hides=0;
static HWND WINAPI testForeground(){return gameWindow;}
static DWORD WINAPI testTick(){return tick;}
static LONG_PTR WINAPI testUserData(HWND,int){return reinterpret_cast<LONG_PTR>(panelPointer);}
static HWND WINAPI testCapture(HWND w){auto old=captureWindow;captureWindow=w;return old;}
static HWND WINAPI testGetCapture(){return captureWindow;}
static BOOL WINAPI testReleaseCapture(){captureWindow=nullptr;if(captureChanged)captureChanged();return TRUE;}
static BOOL WINAPI testInvalidate(HWND,const RECT*,BOOL){++invalidations;return TRUE;}
static BOOL WINAPI testShow(HWND,int mode){if(mode==SW_HIDE)++hides;return TRUE;}
static BOOL WINAPI testClient(HWND,LPRECT rect){*rect={0,0,640,480};return TRUE;}
static BOOL WINAPI testWindow(HWND w){return w==gameWindow||w==panelWindow;}
static BOOL WINAPI testIconic(HWND){return FALSE;}
static BOOL WINAPI testClientScreen(HWND,LPPOINT){return TRUE;}
#define GetForegroundWindow testForeground
#define GetTickCount testTick
#define GetWindowLongPtrW testUserData
#define SetCapture testCapture
#define GetCapture testGetCapture
#define ReleaseCapture testReleaseCapture
#define InvalidateRect testInvalidate
#define ShowWindow testShow
#define GetClientRect testClient
#define IsWindow testWindow
#define IsIconic testIconic
#define ClientToScreen testClientScreen
#include "superweapon_sidebar_test_access.h"
#undef GetForegroundWindow
#undef GetTickCount
#undef GetWindowLongPtrW
#undef SetCapture
#undef GetCapture
#undef ReleaseCapture
#undef InvalidateRect
#undef ShowWindow
#undef GetClientRect
#undef IsWindow
#undef IsIconic
#undef ClientToScreen
struct Memory{
 std::map<uint32_t,uint8_t> data;bool fail=false;
 template<class T>void set(uint32_t p,T value){auto*b=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof value;++i)data[p+uint32_t(i)]=b[i];}
 bool read(uint32_t p,void*out,size_t size){if(fail)return false;auto*b=static_cast<uint8_t*>(out);for(size_t i=0;i<size;++i){auto found=data.find(p+uint32_t(i));if(found==data.end())return false;b[i]=found->second;}return true;}
};
namespace canceltest{
static int armed=-1,writes=0,calls=0;
template<class T>static T get(uint32_t p){assert(p==0x8809A0);return T(armed);}
template<class T>static void put(uint32_t p,T value){assert(p==0x8809A0&&value==-1);armed=int(value);++writes;}
template<class T>static T call(uint32_t function,uint32_t self,void*argument){assert(function==0x4A8BF0&&self==0x87F7E8&&argument==nullptr);++calls;}
#include "../source/superweapon_sidebar_bridge.h"
}
int main(){
 Memory memory;memory.set<uint8_t>(0xA8E378,1);memory.set<int>(0xA8EDA0,0);memory.set<int>(0xA8ED8C,0);constexpr uint32_t house=0x20000,items=0x30000,instance=0x40000,type=0x50000;
 memory.set<uint32_t>(0xA83D4C,house);memory.set<uint32_t>(house+0x258,items);memory.set<int>(house+0x25C,1);memory.set<int>(house+0x264,1);
 memory.set<uint32_t>(items,instance);memory.set<uint32_t>(instance+0x28,type);memory.set<uint32_t>(instance+0x2C,house);memory.set<uint8_t>(instance+0x6D,1);memory.set<uint8_t>(instance+0x6F,1);memory.set<uint8_t>(instance+0x70,0);memory.set<uint8_t>(type+0xF5,0);memory.set<uint8_t>(type+0xE5,0);
 swsidebar::Panel panel;panelPointer=&panel;panel.window_=panelWindow;panel.game_=gameWindow;panel.pid_=1;panel.match_=2;
 panel.options_.enabled=true;panel.clientW_=640;panel.clientH_=480;panel.width_=140;panel.height_=300;panel.sampled_=tick;panel.shown_=panel.inputReady_=true;
 panel.read_=[&](uint32_t p,void*out,size_t n){return memory.read(p,out,n);};
 swsidebar::Entry entry;entry.house=house;entry.instance=instance;entry.type=type;entry.index=0;entry.ready=true;panel.entries_.push_back(entry);
 std::vector<std::tuple<int,uint32_t,uint32_t>>sent;panel.select_=[&](int n,uint32_t p,uint32_t t){sent.emplace_back(n,p,t);};
 captureChanged=[](){swsidebar::Panel::proc(panelWindow,WM_CAPTURECHANGED,0,0);};
 const LPARAM cell=MAKELPARAM(10,32);auto down=[&](){swsidebar::Panel::proc(panelWindow,WM_LBUTTONDOWN,0,cell);};auto up=[&](){swsidebar::Panel::proc(panelWindow,WM_LBUTTONUP,0,cell);};
 up();assert(sent.empty());down();assert(captureWindow==panelWindow);up();assert(sent.size()==1&&sent.back()==std::make_tuple(0,instance,type)&&!captureWindow);
 down();panel.entries_[0].instance+=0x100;up();assert(sent.size()==1);panel.entries_[0]=entry;
 down();memory.set<uint32_t>(items,instance+0x100);up();assert(sent.size()==1);memory.set<uint32_t>(items,instance);
 down();memory.set<uint8_t>(instance+0x70,1);up();assert(sent.size()==1);memory.set<uint8_t>(instance+0x70,0);
 down();memory.fail=true;up();assert(sent.size()==1);memory.fail=false;
 down();overlayvisibility::shown=false;up();assert(sent.size()==1);overlayvisibility::shown=true;
 // A menu can open between press and release, before the next Host tick.
 down();memory.set<int>(0xA8ED8C,1);up();assert(sent.size()==1);
 panel.update(gameWindow,1,2,true,panel.options_,panel.read_,panel.select_,{});assert(!panel.shown_&&!panel.inputReady_);
 memory.set<int>(0xA8ED8C,0);memory.set<int>(0xA8EDA0,1);panel.shown_=panel.inputReady_=true;
 panel.update(gameWindow,1,2,true,panel.options_,panel.read_,panel.select_,{});assert(!panel.shown_);
 memory.set<int>(0xA8EDA0,0);panel.shown_=panel.inputReady_=true;assert(panel.usable());
 down();swsidebar::Panel::proc(panelWindow,WM_CANCELMODE,0,0);up();assert(sent.size()==1&&!captureWindow);
 // An input lease expires before a retained image does; short failure never
 // authorizes a click, then the original 250ms display lease hides it.
 tick=1101;memory.fail=true;panel.update(gameWindow,1,2,true,panel.options_,panel.read_,panel.select_,{});assert(panel.shown_&&!panel.inputReady_);down();up();assert(sent.size()==1);
 tick=1300;panel.update(gameWindow,1,2,true,panel.options_,panel.read_,panel.select_,{});assert(!panel.shown_&&!panel.inputReady_&&hides>0);
 panel.shown_=panel.inputReady_=true;panel.sampled_=tick;memory.fail=false;
 swsidebar::Panel::proc(panelWindow,WM_RBUTTONUP,0,cell);assert(sent.size()==2&&sent.back()==std::make_tuple(-1,0u,0u));
 tick+=250;swsidebar::Panel::proc(panelWindow,WM_RBUTTONUP,0,cell);assert(sent.size()==2);
 // Exercise the actual SHP decoder and panel cache. A changed palette or
 // shape must not retain a picture from the previous native cameo.
 constexpr uint32_t converter=0x60000,palette=0x70000,shape=0x80000;
 memory.set<uint32_t>(0x87F6B0,converter);memory.set<int>(converter+4,2);memory.set<uint32_t>(converter+0x174,palette);
 for(unsigned i=0;i<256;++i)memory.set<uint16_t>(palette+i*2,0);memory.set<uint16_t>(palette+2,0xF800);
 for(unsigned i=0;i<33;++i)memory.set<uint8_t>(shape+i,0);
 memory.set<uint16_t>(shape+2,1);memory.set<uint16_t>(shape+4,1);memory.set<uint16_t>(shape+6,1);memory.set<uint16_t>(shape+12,1);memory.set<uint16_t>(shape+14,1);memory.set<uint32_t>(shape+28,32);memory.set<uint8_t>(shape+32,1);
 panel.entries_[0].shape=shape;panel.loadOneIcon();assert(panel.icons_.at(type).image.pixels==std::vector<uint32_t>{0xFFFF0000});
 memory.set<uint16_t>(palette+2,0x07E0);panel.loadOneIcon();assert(panel.icons_.at(type).image.pixels==std::vector<uint32_t>{0xFF00FF00});
 memory.fail=true;panel.loadOneIcon();assert(panel.icons_.at(type).image.pixels==std::vector<uint32_t>{0xFF00FF00});memory.fail=false;
 panel.entries_[0].shape=shape+0x1000;panel.loadOneIcon();assert(panel.icons_.at(type).image.pixels.empty());
 // A successful empty list is distinct from failed sampling and removes
 // obsolete cached type entries instead of growing an unbounded icon map.
 memory.set<int>(0xA8ED84,100);memory.set<int>(0x8A00A4,640);memory.set<int>(0x8A00A8,480);memory.set<int>(0x886FA0,0);memory.set<int>(0xB0CE30,500);memory.set<int>(house+0x264,0);
 panel.sampled_=tick-101;panel.update(gameWindow,1,2,true,panel.options_,panel.read_,panel.select_,{});assert(panel.entries_.empty()&&panel.icons_.empty()&&!panel.shown_);
 using namespace canceltest;assert(cancelSuperSidebarSelection()==0&&writes==0&&calls==0);armed=3;assert(cancelSuperSidebarSelection()==1&&armed==-1&&writes==1&&calls==1);
 panel.window_=nullptr;captureChanged=nullptr;panelPointer=nullptr;
 std::puts("superweapon actual Panel proc/update: capture, replacement, readiness, read loss, visibility, lease, SHP/palette invalidation, empty-list cache retirement, cancellation and Runtime local-cancel guard passed");
}
