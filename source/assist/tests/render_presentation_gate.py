#!/usr/bin/env python3
"""Execute the real paint entry gate with controlled read/Win32 boundaries.

This checks dispatch and legacy-panel admission, not live Windows rendering.
"""
from pathlib import Path
import argparse
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path,
                    default=Path(__file__).resolve().parents[1] / 'source/main.cpp')
args = parser.parse_args()
text = args.source.read_text('utf-8')
begin = text.index('static void paint(){')
end = text.index('const DWORD renderNow=sampleRenderClock(shared);', begin)
paint = text[begin:end] + '\n ++legacyContinued;\n}\n'
fixture = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using DWORD=uint32_t;
struct RECT {int left=0,top=0,right=0,bottom=0;};
struct POINT {int x=0,y=0;};
namespace external { namespace display {
struct State {bool readable=false;uint8_t tactical=0;int specialDialog=0,dialogCount=0;
 bool visible()const{return readable&&tactical==1&&!specialDialog&&!dialogCount;}};
static State current;
template<class Read>State inspect(Read){return current;}
}}
static bool enabled=true;
static int game=1,legacyContinued=0,updates=0,stops=0,informationStatus=1;
static int oldPanelsHidden=0,informationSamples=0;
static external::display::State received;
struct {int paints=0;} displayCounters;
struct {int renderStatus=1;} snap;
struct {bool readFresh(uint32_t,void*,size_t){return true;}} externalReader;
struct {void suspend(){++oldPanelsHidden;}} overlayRecovery;
static DWORD GetTickCount(){return 1000;}
static bool secondaryDisplayRequested(){return false;}
static void sampleExternal(DWORD){++informationSamples;snap.renderStatus=informationStatus;}
static bool GetClientRect(int,RECT*r){*r={0,0,1024,768};return true;}
static bool ClientToScreen(int,POINT*p){*p={10,20};return true;}
namespace renderupgrade {
static void stop(){++stops;}
static void update(POINT,const RECT&,const external::display::State&s){++updates;received=s;}
// Compile the pre-fix entry too, so its failure is an actual missed dispatch.
static void update(POINT,const RECT&,bool visible){++updates;received={true,uint8_t(visible),0,0};}
}
namespace worldrender {static void invalidate(){++oldPanelsHidden;}}
namespace minimap {static void hide(){++oldPanelsHidden;}}
namespace watchwindows {static void hide(){++oldPanelsHidden;}}
static void hideWorldOverlay(){++oldPanelsHidden;}
static void hideAircraftButton(){++oldPanelsHidden;}
static void hideConstruction(){++oldPanelsHidden;}
static void clearPassengerDisplay(){++oldPanelsHidden;}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
'''
cases = r'''
int main(){
 using external::display::current;
 // The information sampler can fail while the renderer's view remains valid.
 current={true,1,0,0};informationStatus=0;
 paint();CHECK(updates==1&&stops==0);CHECK(received.readable&&received.visible());
 CHECK(legacyContinued==0&&oldPanelsHidden==8);
 // An unreadable display probe must reach the renderer as unknown, not hidden.
 current={false,0,0,0};informationStatus=0;
 paint();CHECK(updates==2&&stops==0&&!received.readable);
 CHECK(legacyContinued==0&&oldPanelsHidden==16);
 // A real dialog is still conveyed immediately; all legacy panels stay hidden.
 current={true,1,1,0};informationStatus=1;
 paint();CHECK(updates==3&&stops==0&&received.readable&&!received.visible());
 CHECK(legacyContinued==0&&oldPanelsHidden==24);
 // Normal updates retain the established admission for every other overlay.
 current={true,1,0,0};informationStatus=1;
 paint();CHECK(updates==4&&stops==0&&legacyContinued==1&&oldPanelsHidden==24);
 CHECK(informationSamples==4&&displayCounters.paints==4);
 // Disabled tactical controls with no secondary request cannot sample here.
 enabled=false;paint();CHECK(informationSamples==4&&updates==5);
 std::puts("PASS: real paint entry dispatches verified/unknown/hidden display states before unrelated information gates; legacy panels unchanged");
}
'''
with tempfile.TemporaryDirectory(prefix='render-presentation-gate-') as tmp:
    root = Path(tmp)
    cpp, exe = root / 'gate.cpp', root / 'gate'
    cpp.write_text(fixture + paint + cases, 'utf-8')
    subprocess.run(['g++', '-std=c++17', '-O2', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
