"""Execute production visibility and interrupted-drag recovery policy.
Foreign foreground never disables/withdraws a live settings page. No live
Windows interaction or game hang claim is made by this portable boundary.
"""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[2]/'suite/source/frontend_panes.h');a=p.parse_args();s=a.source.read_text()
def method(begin,end):return begin+s.split(begin,1)[1].split(end,1)[0]
policy=method(' bool canPresent()const{','\n bool canRaise()')
recover=method(' void reconcileInteraction(){','\n void closing()')
f=r'''
#include <atomic>
#include <cassert>
#include <cstdio>
using HWND=int;constexpr unsigned GUI_INMOVESIZE=2;static bool visible=true,minimized=false,guiOK=true;static unsigned flags=0;static HWND foreground=99;
struct GUITHREADINFO{unsigned size,flags=0;};
static bool IsWindowVisible(HWND w){assert(w==2);return visible;}static bool IsIconic(HWND w){assert(w==2);return minimized;}
static HWND GetForegroundWindow(){return foreground;}
static unsigned GetCurrentThreadId(){return 7;}
static bool GetGUIThreadInfo(unsigned t,GUITHREADINFO*i){assert(t==7);i->flags=flags;return guiOK;}
struct Host{std::atomic<bool>interactive_{false},closing_{false};HWND shell_=2;bool contains(HWND w)const{return w==2||w==3;}
 POLICY
 RECOVER
};
int main(){Host h;
 for(int cycle=0;cycle<10000;++cycle){foreground=99;assert(h.canPresent());foreground=2;assert(h.canPresent());foreground=3;assert(h.canPresent());}
 visible=false;assert(!h.canPresent());visible=true;minimized=true;assert(!h.canPresent());minimized=false;
 h.interactive_=true;flags=GUI_INMOVESIZE;h.reconcileInteraction();assert(!h.canPresent());
 flags=0;h.reconcileInteraction();assert(h.canPresent()); // cancelled drag/Alt-Tab without EXITSIZEMOVE
 h.interactive_=true;guiOK=false;h.reconcileInteraction();assert(!h.canPresent());guiOK=true;h.reconcileInteraction();assert(h.canPresent());
 h.closing_=true;assert(!h.canPresent());std::puts("PASS: 10000 external-app/shell/pane activation cycles preserve native page eligibility; minimize/close guards and interrupted-drag recovery (production methods, controlled API boundary)");}
'''.replace('POLICY',policy).replace('RECOVER',recover)
with tempfile.TemporaryDirectory() as d:
 cpp,exe=Path(d)/'t.cpp',Path(d)/'t';cpp.write_text(f);subprocess.run(['g++','-std=c++17','-O2',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
if 'DwmSetWindowAttribute' in s or 'WM_PRINT' in s:raise AssertionError('Settings input may not depend on DWM cloak or subtree printing')
