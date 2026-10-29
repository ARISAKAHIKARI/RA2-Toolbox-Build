"""Production pane dispatch: modal scroll tracking must not run WM_PRINT.

Runs the actual dispatch/mailbox methods against a controlled Win32 boundary.
This is scheduling/reentrancy coverage, not a Windows desktop hang reproduction.
"""
from pathlib import Path
import argparse, subprocess, tempfile
p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[2] / 'suite/source/frontend_panes.h')
a = p.parse_args()
s = a.source.read_text()
dispatch = ' static LRESULT CALLBACK paneProc' + s.split(' static LRESULT CALLBACK paneProc', 1)[1].split('\n void requestSnapshot', 1)[0]
request = ' void requestSnapshot' + s.split('\n void requestSnapshot', 1)[1].split('\n void capture', 1)[0]
fixture = r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <initializer_list>
#define CALLBACK
using HWND=void*;using UINT=unsigned;using UINT_PTR=uintptr_t;using DWORD_PTR=uintptr_t;
using WPARAM=uintptr_t;using LPARAM=intptr_t;using LRESULT=intptr_t;
constexpr UINT WM_TIMER=0x113,WM_NCDESTROY=0x82,WM_ACTIVATE=6,WM_SHOWWINDOW=0x18,
 WM_COMMAND=0x111,WM_VSCROLL=0x115,WM_HSCROLL=0x114,WM_MOUSEWHEEL=0x20a,WM_APP=0x8000;
constexpr UINT SnapshotMessage=WM_APP+88,ActivityMessage=WM_APP+87,DestroyedMessage=WM_APP+89;
constexpr UINT_PTR SnapshotTimer=0x6329;constexpr UINT SnapshotDelay=120;
constexpr UINT GUI_INMOVESIZE=2,GUI_INMENUMODE=4,GUI_SYSTEMMENUMODE=8,GUI_POPUPMENUMODE=16;
struct GUITHREADINFO {unsigned size,flags=0;HWND hwndCapture=nullptr;};
static unsigned flags=0,prints=0,inputs=0,arms=0,kills=0,posted=0;
static HWND capture=nullptr;static bool guiOK=true,timerOK=true;
static unsigned GetCurrentThreadId(){return 12;}
static bool GetGUIThreadInfo(unsigned tid,GUITHREADINFO*out){assert(tid==12);out->flags=flags;out->hwndCapture=capture;return guiOK;}
static UINT_PTR SetTimer(HWND w,UINT_PTR id,UINT delay,void*){assert(w&&id==SnapshotTimer&&delay==120);++arms;return timerOK?id:0;}
static bool KillTimer(HWND,UINT_PTR id){assert(id==SnapshotTimer);++kills;return true;}
static bool PostMessageW(HWND,UINT,WPARAM,LPARAM){++posted;return true;}
static void RemovePropW(HWND,const wchar_t*){}
template<class F>static void RemoveWindowSubclass(HWND,F,UINT_PTR){}
static LRESULT DefSubclassProc(HWND,UINT m,WPARAM,LPARAM){if(m==WM_MOUSEWHEEL||m==WM_VSCROLL||m==WM_HSCROLL||m==WM_COMMAND)++inputs;return 7;}
class Host {
 struct Slot {std::atomic<HWND>window{reinterpret_cast<HWND>(1)};std::mutex mutex;bool pending=false,appliedValid=false,snapshotPending=false,capturing=false;std::shared_ptr<int>snapshot;Host*host=nullptr;unsigned index=0;} slot;
 HWND shell_=reinterpret_cast<HWND>(2);
 DISPATCH
 REQUEST
 void capture(Slot& s){assert(!::capture&&!flags);assert(!s.snapshotPending);++prints;}
 public:
 Host(){slot.host=this;}
 LRESULT send(UINT m,WPARAM a=0){return paneProc(slot.window.load(),m,a,0,1,reinterpret_cast<DWORD_PTR>(&slot));}
 bool pending()const{return slot.snapshotPending;}
};
int main(){
 Host host;capture=reinterpret_cast<HWND>(1);
 // A stale posted snapshot may be dispatched by the scrollbar's modal loop.
 host.send(SnapshotMessage);assert(prints==0&&host.pending());
 for(unsigned i=0;i<1000;++i){assert(host.send(i%2?WM_MOUSEWHEEL:WM_VSCROLL)==7);host.send(WM_TIMER,SnapshotTimer);}
 assert(inputs==1000&&prints==0&&host.pending());
 capture=nullptr;
 for(unsigned f:{GUI_INMOVESIZE,GUI_INMENUMODE,GUI_SYSTEMMENUMODE,GUI_POPUPMENUMODE}){flags=f;host.send(WM_TIMER,SnapshotTimer);assert(prints==0&&host.pending());}
 flags=0;guiOK=false;host.send(WM_TIMER,SnapshotTimer);assert(prints==0&&host.pending());guiOK=true;
 host.send(WM_TIMER,SnapshotTimer);assert(prints==1&&!host.pending());
 host.send(WM_TIMER,SnapshotTimer);assert(prints==1); // stale killed-timer message
 assert(host.send(WM_COMMAND)==7);assert(inputs==1001&&prints==1&&host.pending());
 host.send(WM_TIMER,SnapshotTimer);assert(prints==2&&!host.pending());
 timerOK=false;host.send(WM_HSCROLL);assert(!host.pending()&&prints==2);timerOK=true;
 host.send(WM_MOUSEWHEEL);assert(host.pending());host.send(WM_NCDESTROY);assert(!host.pending());
 std::puts("PASS: production pane dispatch defers snapshots through 1000 scrolling messages, capture/menu/move loops and GUI-query failure; button dispatch, idle recovery, timer failure and destruction remain responsive (controlled boundary only)");
}
'''.replace('DISPATCH',dispatch).replace('REQUEST',request)
with tempfile.TemporaryDirectory(prefix='frontend-snapshot-') as d:
    cpp,exe=Path(d)/'test.cpp',Path(d)/'test'
    cpp.write_text(fixture)
    subprocess.run(['g++','-std=c++17','-O2','-UNDEBUG','-pthread',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
