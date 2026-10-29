"""Execute production layout dedup against restored/minimized/covered HWNDs."""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[2]/'suite/source/frontend_panes.h');a=p.parse_args();s=a.source.read_text()
request=' bool request('+s.split(' bool request(',1)[1].split('\n bool apply(',1)[0]
f=r'''
#include <array>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <mutex>
using HWND=int;using UINT=unsigned;using WPARAM=uintptr_t;using LPARAM=intptr_t;
constexpr UINT LayoutMessage=100;
struct Layout {int rect=1;bool visible=true;unsigned dpi=96;uint64_t order=1;bool operator==(const Layout&b)const{return rect==b.rect&&visible==b.visible&&dpi==b.dpi&&order==b.order;}};
static bool shown=true,iconic=false,aboveShell=true,raise=true;static unsigned posts=0;
static bool IsWindowVisible(HWND w){assert(w==1);return shown;}static bool IsIconic(HWND w){assert(w==1);return iconic;}
static bool PostMessageW(HWND w,UINT m,WPARAM,LPARAM){assert(w==1&&m==LayoutMessage);++posts;return true;}
struct Host {
 struct Slot {std::atomic<HWND>window{1};std::mutex mutex;Layout latest{},applied{},executing{};bool pending=false,processing=false,appliedValid=true;};
 std::array<Slot,3>slots_{};HWND shell_=2;bool canRaise(){return raise;}static bool above(HWND w,HWND shell){assert(w==1&&shell==2);return aboveShell;}
 REQUEST
 void applied(){slots_[0].pending=false;slots_[0].processing=false;slots_[0].applied=slots_[0].latest;}
};
int main(){Host h;Layout wanted;
 assert(h.request(0,wanted)&&posts==0); // live, correctly ordered stable page
 aboveShell=false;assert(h.request(0,wanted)&&posts==1);h.applied();aboveShell=true;
 iconic=true;assert(h.request(0,wanted)&&posts==2);h.applied();iconic=false;
 shown=false;assert(h.request(0,wanted)&&posts==3);h.applied();shown=true;
 for(unsigned i=0;i<1000;++i){aboveShell=false;raise=false;assert(h.request(0,wanted)&&posts==3+i);raise=true;assert(h.request(0,wanted)&&posts==4+i);h.applied();aboveShell=true;}
 // Clicking the already selected section forces a new owner-thread layout.
 for(unsigned i=0;i<20;++i){++wanted.order;assert(h.request(0,wanted)&&posts==1004+i);h.applied();assert(h.request(0,wanted)&&posts==1004+i);}
 std::puts("PASS production layout request: visible-but-covered, iconic and hidden pages recover without changing geometry/order; 1000 restore cycles and no foreground stealing");}
'''.replace('REQUEST',request)
with tempfile.TemporaryDirectory() as d:
 cpp,exe=Path(d)/'t.cpp',Path(d)/'t';cpp.write_text(f);subprocess.run(['g++','-std=c++17','-O2',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
