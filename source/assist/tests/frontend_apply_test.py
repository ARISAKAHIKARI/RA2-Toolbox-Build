"""Execute production request/apply across hidden, foreground-lag and switches."""
from pathlib import Path
import argparse,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=root/'suite/source/frontend_panes.h');a=p.parse_args();s=a.source.read_text()
layout='struct Layout {'+s.split('struct Layout {',1)[1].split('\nclass Host',1)[0]
request=' bool request('+s.split(' bool request(',1)[1].split('\n bool apply(',1)[0]
apply=' bool apply('+s.split(' bool apply(',1)[1].split('\n};',1)[0]
body=r'''
#include <array>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <functional>
#include <mutex>
using HWND=uintptr_t;using UINT=unsigned;using WPARAM=uintptr_t;using LPARAM=intptr_t;
struct RECT{int left=0,top=0,right=500,bottom=400;};
constexpr UINT LayoutMessage=100,CategoryMessage=101,DpiMessage=102,WM_SIZE=5,SIZE_RESTORED=0;
constexpr UINT SWP_NOACTIVATE=1,SWP_NOOWNERZORDER=2,SWP_SHOWWINDOW=4,SWP_NOZORDER=8,SWP_NOMOVE=16,SWP_NOSIZE=32;
constexpr int GW_HWNDPREV=1,SW_SHOWNOACTIVATE=4,SW_HIDE=0;
constexpr unsigned RDW_INVALIDATE=1,RDW_ERASE=2,RDW_FRAME=4,RDW_ALLCHILDREN=8,RDW_UPDATENOW=16;
constexpr HWND HWND_TOP=0;
constexpr int FALSE=0;
#define MAKELPARAM(a,b) ((uintptr_t(a)&65535)|(uintptr_t(b)<<16))
static bool shown=false,iconic=false,zcorrect=false,foreground=false,present=true,placeOK=true;
static unsigned posts=0,categories=0,paints=0,lastFlags=0;static int displayed=-1;static HWND lastAnchor=0;
static std::function<void()> onDpi;
static bool IsWindowVisible(HWND){return shown;}static bool IsIconic(HWND){return iconic;}
static bool PostMessageW(HWND,UINT message,WPARAM,LPARAM){if(message==LayoutMessage)++posts;return true;}
static void ShowWindow(HWND,int mode){shown=mode!=SW_HIDE;iconic=false;}
static bool GetWindowRect(HWND,RECT*r){*r={};return true;}static bool GetClientRect(HWND,RECT*r){*r={};return true;}
static HWND GetWindow(HWND shell,int relation){assert(shell==2&&relation==GW_HWNDPREV);return zcorrect?1:3;}
static bool SetWindowPos(HWND w,HWND anchor,int,int,int,int,UINT flags){assert(w==1);lastAnchor=anchor;lastFlags=flags;if(!placeOK)return false;shown=true;zcorrect=true;return true;}
static void SendMessageW(HWND w,UINT message,WPARAM value,LPARAM){assert(w==1);if(message==DpiMessage){if(onDpi)onDpi();}else if(message==CategoryMessage){++categories;displayed=int(value);}else assert(message==WM_SIZE);}
static void RedrawWindow(HWND,void*,void*,UINT){++paints;}
LAYOUT
struct Host{
 struct Slot{std::atomic<HWND>window{1};std::mutex mutex;Layout latest{},applied{},executing{};bool pending=false,processing=false,appliedValid=false;unsigned dpi=96,index=0;};
 std::array<Slot,3>slots_{};HWND shell_=2;
 Slot*slot(HWND w){return w==1?&slots_[0]:nullptr;}
 bool canRaise(){return foreground;}bool canPresent(){return present;}
 static bool above(HWND,HWND){return zcorrect;}
 REQUEST
 APPLY
};
int main(){Host h;Layout l{{0,0,500,400},true,96,1};l.category=0;
 // A selected hidden page MUST be queued even during foreground activation lag.
 assert(h.request(0,l)&&posts==1);assert(h.apply(1,LayoutMessage,DpiMessage));
 assert(shown&&displayed==0&&categories==1&&paints&&lastAnchor==3&&(lastFlags&SWP_NOACTIVATE));
 for(int i=0;i<1000;++i){l.category=i%3;++l.order;assert(h.request(0,l));h.apply(1,LayoutMessage,DpiMessage);assert(shown&&displayed==i%3);}
 auto before=posts;l.category=0;++l.order;h.request(0,l);l.category=2;++l.order;h.request(0,l);
 assert(posts==before+1);h.apply(1,LayoutMessage,DpiMessage);assert(displayed==2);
 // DPI work may re-enter and receive a newer hide. The old show must lose.
 auto hidden=l;hidden.visible=false;l.dpi=144;++l.order;
 h.request(0,l);onDpi=[&]{h.request(0,hidden);};h.apply(1,LayoutMessage,DpiMessage);onDpi={};assert(!shown);
 h.apply(1,LayoutMessage,DpiMessage);h.request(0,l);h.apply(1,LayoutMessage,DpiMessage);assert(shown&&displayed==2);
 shown=false;zcorrect=false;placeOK=false;++l.order;h.request(0,l);h.apply(1,LayoutMessage,DpiMessage);assert(!shown);
 placeOK=true;h.request(0,l);h.apply(1,LayoutMessage,DpiMessage);assert(shown&&lastAnchor==3);
 std::puts("PASS production request/apply: hidden foreground-lag page, 1000 category switches, latest-only category, reentrant hide and failed-position retry");}
'''.replace('LAYOUT',layout).replace('REQUEST',request).replace('APPLY',apply)
# ActivityMessage is only a nonblocking completion notification.
body=body.replace('constexpr UINT LayoutMessage=100,','constexpr UINT ActivityMessage=99,LayoutMessage=100,')
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.cpp').write_text(body)
 subprocess.run(['g++','-std=c++17','-O2',str(p/'t.cpp'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
