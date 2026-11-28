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
lease_begin=text.index('static overlaydisplay::ReadLease informationDisplayLease;')
lease_end=text.index('#include "construction_host.h"',lease_begin)
lease=text[lease_begin:lease_end]
lease_header=(args.source.parents[2]/'shared/source/overlay_display_lease.h').resolve().as_posix()
projection_header=(args.source.parents[2]/'shared/source/native_viewport_projection.h').resolve().as_posix()
fixture = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <initializer_list>
#include <memory>
#include "LEASE_HEADER"
#include "PROJECTION_HEADER"
using DWORD=uint32_t;
struct RECT {int left=0,top=0,right=0,bottom=0;};
struct POINT {int x=0,y=0;};
namespace external { namespace display {
struct State {bool readable=false;uint8_t tactical=0;int specialDialog=0,dialogCount=0;
 bool visible()const{return readable&&tactical==1&&!specialDialog&&!dialogCount;}};
static State current;static unsigned probes=0;
template<class Read>State inspect(Read){++probes;return current;}
}}
static bool enabled=true,foreground=true;
static uint32_t pid=77,match=8,clockNow=1000;
static bool foregroundGame(){return foreground;}
static bool secondaryDisplayAvailable(){return false;}
static uint32_t displayMatch(){return match;}
static int game=1,legacyContinued=0,updates=0,stops=0,informationStatus=1;
static int oldPanelsHidden=0,informationSamples=0,touches=0,sidebarTouches=0,additionalHides=0;
static struct {unsigned previousPid=0,previousMatch=0,observed=0,holdDrop=0;int x=0,y=0;bool valid=false;} cameraDiagnostics;
static void logCameraDiagnostics(DWORD){}
static void critical(const std::wstring&){}
namespace pipelineperf {
struct Window {Window(const wchar_t*,std::initializer_list<const wchar_t*>){}};
struct Scope {Scope(Window&,int,void(*)(const std::wstring&)=nullptr){}};
}
namespace overlayvisibility {static bool visible(){return true;}}
namespace unitpanelhost {static struct {void hide(){++additionalHides;}}panel;}
namespace watchhost {static void suspend(){}}
namespace sidebaroverlay {static constexpr uint64_t Layer=2;}
namespace nativeviewhost {
 using Transform=nativeviewportprojection::Transform;
 static bool sampleTransform(void*,Transform&out){out={};return true;}
 static uint64_t key(const Transform&){return 0;}
 static nativeviewportprojection::Rect clip(const Transform&,int w,int h){return {0,0,w,h};}
}
static void*shared=nullptr;
static uint32_t paintRevision=0;
static struct {void projection(uint32_t){}} motion,statusMotion,enemyMotion;
static struct {
 nativeviewhost::Transform sampledViewport()const{return {};}
} externalSampler;
static external::display::State received;
struct {int paints=0;} displayCounters;
struct {int renderStatus=1,width=1024,height=768,cameraX=0,cameraY=0;uint32_t match=8;bool chat=false;} snap;
struct {bool readFresh(uint32_t,void*,size_t){return true;}} externalReader;
struct {void suspend(){++oldPanelsHidden;}} overlayRecovery;
static DWORD GetTickCount(){return clockNow;}
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
namespace overlaycompositor {constexpr uint64_t World=1;struct Host {bool touch(uint64_t id,uintptr_t,uint32_t){++(id==World?touches:sidebarTouches);return true;}};static Host&host(){static Host h;return h;}}
namespace worldrender {
struct Frame {struct {int cameraX=0,cameraY=0;} context;};
static std::shared_ptr<Frame> displayed;
static void invalidate(){++oldPanelsHidden;displayed.reset();}
}
namespace minimap {static void hide(){++oldPanelsHidden;}static void hidePrimary(){++oldPanelsHidden;}}
namespace watchwindows {static void hide(){++oldPanelsHidden;}static void hidePrimary(){++oldPanelsHidden;}}
static void hideWorldOverlay(){++oldPanelsHidden;}
static void hideSidebarOverlay(){++additionalHides;}
static void hideAircraftButton(){++oldPanelsHidden;}
static void hideConstruction(){++oldPanelsHidden;}
static void clearPassengerDisplay(){++oldPanelsHidden;}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
'''
cases = r'''
int main(){
 using external::display::current;
 current={true,1,0,0};informationStatus=1;
 paint();CHECK(updates==1&&stops==0&&legacyContinued==1&&oldPanelsHidden==2);
 oldPanelsHidden=0; // The first viewport identity invalidates any older surfaces.
 worldrender::displayed=std::make_shared<worldrender::Frame>();
 // Failed information is not a known menu. Keep existing pixels only.
 informationStatus=0;clockNow+=15;paint();
 CHECK(updates==2&&legacyContinued==1&&oldPanelsHidden==0&&touches==1);
 current={false,0,0,0};clockNow+=15;paint();
 CHECK(updates==3&&!received.readable&&legacyContinued==1&&oldPanelsHidden==0&&touches==2);
 // Repeated failures cannot renew the120ms deadline.
 for(int n=0;n<4;++n){clockNow+=15;paint();CHECK(oldPanelsHidden==0);}
 clockNow=1120;paint();CHECK(oldPanelsHidden==8&&legacyContinued==1);
 // A readable native dialog cancels immediately, without waiting for grace.
 current={true,1,0,0};informationStatus=1;clockNow=1200;paint();
 current={true,1,1,0};clockNow++;paint();CHECK(oldPanelsHidden==16);
 // A good sample cannot authorize a different window/epoch or changed size.
 current={true,1,0,0};clockNow=1300;paint();
 informationStatus=0;++pid;clockNow++;paint();CHECK(oldPanelsHidden==24);
 informationStatus=1;clockNow++;paint();informationStatus=0;++match;clockNow++;paint();CHECK(oldPanelsHidden==32);
 informationStatus=1;clockNow++;paint();informationStatus=0;++snap.width;clockNow++;paint();CHECK(oldPanelsHidden==40);
 informationStatus=1;clockNow++;paint();informationStatus=0;foreground=false;clockNow++;paint();CHECK(oldPanelsHidden==48);
 foreground=true;informationStatus=1;clockNow++;paint();informationStatus=0;snap.chat=true;clockNow++;paint();CHECK(oldPanelsHidden==56);snap.chat=false;
 // Tick wrap keeps the same bounded deadline; disabled display is immediate.
 informationStatus=1;clockNow=0xfffffff0u;paint();informationStatus=0;clockNow=20;paint();CHECK(oldPanelsHidden==56);
 clockNow=104;paint();CHECK(oldPanelsHidden==64);
 int samples=informationSamples;enabled=false;clockNow++;paint();CHECK(informationSamples==samples&&oldPanelsHidden==72);
 // Hotkeys, lanes, hover and input share a good8ms observation.
 enabled=foreground=true;snap.renderStatus=1;current={true,1,0,0};clockNow=2000;
 unsigned probes=external::display::probes;
 CHECK(constructionDisplayFresh()&&external::display::probes==probes+1);
 for(int n=0;n<100;++n)CHECK(constructionDisplayVisible()&&constructionDisplayFresh());
 CHECK(external::display::probes==probes+1);
 clockNow+=7;CHECK(constructionDisplayFresh()&&external::display::probes==probes+1);
 clockNow++;CHECK(constructionDisplayFresh()&&external::display::probes==probes+2);
 ++match;CHECK(constructionDisplayFresh()&&external::display::probes==probes+3);
 snap.renderStatus=0;CHECK(!constructionDisplayFresh()&&constructionDisplayVisible());
 current={false,0,0,0};clockNow+=100;CHECK(!constructionDisplayFresh()&&constructionDisplayVisible());
 clockNow+=20;CHECK(!constructionDisplayVisible());
 snap.renderStatus=1;current={true,1,0,0};clockNow++;CHECK(constructionDisplayFresh());
 current={true,1,1,0};RECT client{0,0,1024,768};
 CHECK(passiveDisplayState(current,client)==overlaydisplay::State::Hidden&&!constructionDisplayVisible());
 current={true,1,0,0};clockNow=3000;informationStatus=1;paint();
 worldrender::displayed=std::make_shared<worldrender::Frame>();
 const int hidden=oldPanelsHidden,worldTouches=touches,sideTouches=sidebarTouches;
 snap.cameraX=42;informationStatus=0;clockNow+=20;paint();
 CHECK(oldPanelsHidden==hidden+1&&touches==worldTouches&&sidebarTouches==sideTouches+1&&cameraDiagnostics.holdDrop==1);
 std::puts("PASS: actual paint/display lease retains same-camera pixels, expires at 120ms, and hides stale world pixels independently of the sidebar");
}
'''

with tempfile.TemporaryDirectory(prefix='render-presentation-gate-') as tmp:
    root = Path(tmp)
    cpp, exe = root / 'gate.cpp', root / 'gate'
    fixture_text = fixture.replace('LEASE_HEADER',lease_header).replace('PROJECTION_HEADER',projection_header)
    cpp.write_text(fixture_text + lease + paint + cases, 'utf-8')
    subprocess.run(['g++', '-std=c++17', '-O2', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
