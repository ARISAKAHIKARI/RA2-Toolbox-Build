#!/usr/bin/env python3
"""Exercise production statistics tick and second-display host with Win32 doubles.

The actual game reader, GDI raster and physical monitor are not exercised here.
"""
from pathlib import Path
import json
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
source=(root/'unit_monitor/source/main.cpp').read_text()
def function(signature):
 start=source.index(signature);end=source.index('{',start)+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]

prefix=r'''
#include <algorithm>
#include <atomic>
#define LONG_MIN (-2147483647-1)
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cassert>
#include <iostream>
using DWORD=uint32_t;using BYTE=uint8_t;using LONG=int;using HWND=void*;using HDC=void*;using BOOL=int;
struct RECT {LONG left=0,top=0,right=0,bottom=0;};struct POINT {LONG x=0,y=0;};struct SIZE {LONG cx=0,cy=0;};
struct BLENDFUNCTION {BYTE BlendOp,BlendFlags,SourceConstantAlpha,AlphaFormat;};
constexpr int SW_HIDE=0,SWP_NOMOVE=1,SWP_NOSIZE=2,SWP_NOACTIVATE=4,SWP_SHOWWINDOW=8,AC_SRC_OVER=0,AC_SRC_ALPHA=1,ULW_ALPHA=2;
static HWND handle(uintptr_t n){return reinterpret_cast<HWND>(n);}
static DWORD now=1000;static bool focused=false,battleVisible=true,processAlive=true,readerValid=true;
static HWND game=handle(1),ui=handle(2),overlay=handle(3),secondaryOverlay=handle(4);static DWORD pid=10,threadId=11;
static std::map<HWND,bool> visible;static unsigned uploads=0,placements=0,primaryRenders=0,primaryPresents=0,primaryVisibility=0,samples=0;
static POINT uploadedOrigin;static SIZE uploadedSize;static bool primaryReportedVisible=false;
static DWORD GetTickCount(){return now;}static bool IsWindow(HWND w){return w==game||w==ui||w==overlay||w==secondaryOverlay;}
static void ShowWindow(HWND w,int command){visible[w]=command!=SW_HIDE;}
static BOOL IsWindowVisible(HWND w){return visible[w];}
static unsigned GetWindowThreadProcessId(HWND,DWORD*out){*out=pid;return threadId;}
static bool foregroundGame(){return focused;}
struct MONITORINFO {DWORD cbSize;RECT rcMonitor;};constexpr int MONITOR_DEFAULTTONEAREST=2;static void* MonitorFromWindow(HWND,int){return handle(10);}static bool GetMonitorInfoW(void*,MONITORINFO*m){m->rcMonitor={0,0,1920,1080};return true;}
static void GetClientRect(HWND,RECT*r){*r={0,0,1920,1080};}static void ClientToScreen(HWND,POINT*p){*p={0,0};}
static BOOL UpdateLayeredWindow(HWND w,void*,POINT*p,SIZE*s,HDC,POINT*,int,BLENDFUNCTION*b,int){assert(w==secondaryOverlay&&b->SourceConstantAlpha==255);++uploads;uploadedOrigin=*p;uploadedSize=*s;return 1;}
namespace overlaystack {enum class Layer {Statistics};static bool place(HWND w,Layer,int,int,int,int,int flags){assert(flags&SWP_NOACTIVATE);if(flags&SWP_SHOWWINDOW)visible[w]=true;++placements;return true;}}
namespace secondarydisplay {struct Target {void*monitor=nullptr;RECT work;bool available()const{return monitor&&work.right>work.left&&work.bottom>work.top;}};static Target selected{handle(20),{-1600,-100,0,800}};struct Monitor {Target get(HWND,HWND,DWORD){return selected;}};}
static secondarydisplay::Monitor secondaryMonitor;
namespace um {constexpr int MaxTypes=64,MaxPlayers=8;struct Snapshot {int status=1,typeCount=1,playerCount=1,paletteReady=1,missingIcons=0,readErrors=0;uint32_t match=7,tick=1000;};}
static std::unique_ptr<um::Snapshot> snap(new um::Snapshot),incoming(new um::Snapshot);
static int desiredStatus=1;static uint32_t desiredMatch=7;
struct Sampler {um::Snapshot sample(DWORD t,bool){++samples;um::Snapshot s;s.status=desiredStatus;s.match=desiredMatch;s.tick=t;return s;}struct Probe {int stage=0,address=0;};Probe probe(){return {};}};
static std::unique_ptr<Sampler> sampler(new Sampler);
struct Reader {int lastReadError=0;bool alive(){return processAlive;}bool valid(){return readerValid;}void begin(){}bool readFresh(uint32_t,void*out,size_t n){if(n==4)*static_cast<int*>(out)=1920;return true;}} reader;
namespace external {
namespace display {struct State {bool readable=true;int tactical=1,specialDialog=0,dialogCount=0;bool visible()const{return battleVisible;}};template<class F>State inspect(F){return {};}}
namespace layout {static std::wstring description(int){return L"probe";}}
namespace discovery {struct Found {struct {HWND window;DWORD pid=10,thread=11;} best;};static HWND resolveYrppWindow(Reader&,DWORD,HWND,DWORD*t){*t=threadId;return game;}static Found scan(DWORD){return {{game,pid,threadId}};}}
}
struct Recovery {template<class T>bool due(DWORD,bool,T,bool){return false;}void suspend(){}void submitted(bool){}};
struct RecoveryKey {uintptr_t window;DWORD pid;uint32_t match;int width,height;};
struct ExactRecovery {bool due(DWORD,bool,RecoveryKey,bool shown){return !shown;}void suspend(){}void submitted(bool){}};
static ExactRecovery secondaryRecovery,overlayRecovery;
struct UnitSurface {HDC dc=nullptr;int w=0,h=0;std::vector<uint32_t>data;void*bits=nullptr;void clear(){dc=nullptr;w=h=0;data.clear();bits=nullptr;}bool ensure(int width,int height){w=width;h=height;data.resize(size_t(w)*h);bits=data.data();dc=handle(30);return true;}} surface,secondarySurface;
static bool enabled=false,demo=false,secondaryEnabled=true,secondaryActive=false,secondaryDirty=true,renderDirty=true,gameActive=false,showOthers=true,buildings=false,hidden=true,manualVisible=false,tabEnabled=true;
static int page=0,scalePercent=100,leftMargin=6,topMargin=20,paintedAlpha=-1;
static int secondaryX=0,secondaryY=0;
static DWORD lastSearch=0,lastGood=1000,lastData=0,oldMatch=7;
static bool changed(const um::Snapshot&a,const um::Snapshot&b){return a.status!=b.status||a.match!=b.match||a.playerCount!=b.playerCount;}
static std::wstring status;static void setStatus(const std::wstring&s){status=s;}static void visibilityStatus(const std::wstring&){}
static void log(const wchar_t*){}static void pollTab(){}static void refreshInformationRate(DWORD){}static void demoData(){}
static int statsInformationFPS(){return 144;}
struct Clock {bool active=false;void state(bool on,int){active=on;}bool running(){return false;}} statsFrameClock;
static void statsVisible(bool value){++primaryVisibility;primaryReportedVisible=value;}
namespace tabui {static bool displayRequested(bool hidden,bool,bool,bool){return !hidden;}}
static struct Tabs {void loseFocus(){}bool visible(){return false;}}tabs;
static struct Fade {bool desired=false;void hide(){desired=false;}void target(bool on,DWORD){desired=on;}int value(DWORD){return desired?255:0;}}fade;
namespace viewport {struct Scale {bool set(int,int,int w,int h){return w>0&&h>0;}};}
'''
render_structs=source[source.index('struct RenderRequest {'):source.index('static std::shared_ptr<const RenderFrame> renderPixels')]
host=r'''
struct Worker {std::shared_ptr<const RenderFrame> frame;unsigned submitted=0,invalidated=0;void invalidate(){frame.reset();++invalidated;}void submit(RenderRequest r){++submitted;auto next=std::make_shared<RenderFrame>();next->request=std::move(r);next->width=12;next->height=9;next->pixels.assign(12*9,0xff123456);frame=next;}auto get(){return frame;}} secondaryRenderWorker;
static std::shared_ptr<const RenderFrame> secondaryRenderedFrame;
static POINT secondaryOrigin{-2147483647,-2147483647};static int secondaryWidth=0,secondaryHeight=0;
static bool secondaryUploaded=false;static DWORD secondaryLastSubmit=0;
'''
placement=source[source.index('static POINT secondaryPosition('):source.index('static SecondaryDrag secondaryDrag;')]
drag_host='static SecondaryDrag secondaryDrag;\nstatic void resetSecondary();\nstatic void finishSecondaryDrag(){secondaryDrag.active=false;}\n'
post=r'''
static void detach(){resetSecondary();game=nullptr;pid=threadId=0;sampler.reset();*snap={};snap->status=0;lastGood=0;}
static void attach(){if(!processAlive)return;game=handle(1);pid=10;threadId=11;sampler=std::make_unique<Sampler>();}
static struct Cache {std::atomic<unsigned> loaded{0};bool writeFailed=false;}iconCache;
static void render(int,int,POINT,int,int){++primaryRenders;renderDirty=false;}static void present(POINT,int,int,int,int,int,bool){++primaryPresents;statsVisible(true);visible[overlay]=true;}
'''
scenario=r'''
int main(){
 // Primary module is off, Tab is hidden, game is not foreground: one sampler
 // still feeds the independent display and its frame-clock eligibility.
 tick(false);assert(samples==1&&statsFrameClock.active&&primaryRenders==0&&!visible[overlay]);
 ++now;tick(true);assert(uploads==1&&visible[secondaryOverlay]&&!visible[overlay]&&!primaryReportedVisible);
 assert(uploadedOrigin.x==-1600&&uploadedOrigin.y==-100&&secondaryWidth==960&&secondaryHeight==900);
 UnitSurface grip;assert(grip.ensure(96,56));paintSecondaryDragGrip(grip);
 auto* gripPixels=static_cast<uint32_t*>(grip.bits);
 assert(gripPixels[3*96+8]==0xff6488a4u);
 assert(secondaryRenderedFrame->request.snap->match==7&&secondaryRenderedFrame->request.width==960);
 const unsigned queued=secondaryRenderWorker.submitted;for(int n=0;n<100;++n){++now;tick(true);}assert(secondaryRenderWorker.submitted==queued&&uploads==1);
 now=1300;tick(false);assert(samples==2&&secondaryActive&&statsFrameClock.active);
 // Coordinates anchor the actually displayed sparse frame (12 x 9), not the
 // requested 960 x 900 rendering budget. Right/bottom means actual edges.
 secondaryX=10000;secondaryY=10000;++now;tick(true);
 assert(secondaryWidth==960&&secondaryHeight==900&&uploadedOrigin.x==-12&&uploadedOrigin.y==791);
 assert(uploadedOrigin.x+uploadedSize.cx<=secondarydisplay::selected.work.right&&
        uploadedOrigin.y+uploadedSize.cy<=secondarydisplay::selected.work.bottom);
 unsigned movedSubmit=secondaryRenderWorker.submitted,movedUploads=uploads;
 secondaryX=secondaryY=0;++now;tick(true);
 assert(uploadedOrigin.x==-1600&&uploadedOrigin.y==-100&&uploads==movedUploads+1);
 assert(secondaryRenderWorker.submitted==movedSubmit&&"position-only edits reuse completed pixels");
 SecondaryDrag sample;sample.active=true;sample.work={-1600,-100,0,800};
 sample.cursor={100,100};sample.origin={-1600,-100};sample.x=sample.y=0;sample.width=12;sample.height=9;
 auto farCorner=secondaryDragSettings(sample,{5000,5000});
 assert(farCorner.x==10000&&farCorner.y==10000);
 auto actualCorner=secondaryPosition(sample.work,12,9,farCorner.x,farCorner.y);
 assert(actualCorner.x==-12&&actualCorner.y==791);
 sample.x=sample.y=10000;sample.origin={-12,791};
 auto back=secondaryDragSettings(sample,{-5000,-5000});assert(back.x==0&&back.y==0);
 auto bounded=secondaryPosition(sample.work,12,9,12000,-10);
 assert(bounded.x==-12&&bounded.y==-100);
 // Primary can independently appear and disappear without changing the copy.
 enabled=true;focused=true;hidden=false;++now;tick(true);assert(primaryRenders&&primaryPresents&&visible[overlay]&&visible[secondaryOverlay]&&primaryReportedVisible);
 enabled=false;focused=false;hidden=true;++now;tick(true);assert(!visible[overlay]&&visible[secondaryOverlay]&&!primaryReportedVisible&&statsFrameClock.active);
 // Menu visibility invalidates cached pixels; no opaque old match survives.
 battleVisible=false;++now;tick(true);assert(!secondaryActive&&!visible[secondaryOverlay]&&!secondaryRenderedFrame&&!statsFrameClock.active);
 battleVisible=true;++now;tick(true);assert(secondaryActive&&visible[secondaryOverlay]&&statsFrameClock.active);
 // Hot-unplug has no fallback on the game's monitor, and reconnect rebuilds.
 secondarydisplay::selected={};++now;tick(true);assert(!visible[secondaryOverlay]&&!secondaryRenderedFrame&&!statsFrameClock.active);
 secondarydisplay::selected={handle(21),{1920,0,3840,1040}};++now;tick(true);assert(visible[secondaryOverlay]&&uploadedOrigin.x==1920&&uploadedOrigin.y==0&&secondaryWidth==1152&&secondaryHeight==1040);
 // A round transition rejects the previous asynchronous frame before publish.
 desiredMatch=8;now=1600;tick(true);assert(samples==3&&oldMatch==8&&secondaryRenderedFrame->request.match==8);
 secondaryEnabled=false;++now;tick(true);assert(!secondaryActive&&!visible[secondaryOverlay]&&!statsFrameClock.active);
 unsigned count=samples;now=1900;tick(false);assert(samples==count);
 secondaryEnabled=true;now=2200;tick(true);assert(samples==count+1&&visible[secondaryOverlay]);
 processAlive=false;now=2500;tick(true);assert(!visible[secondaryOverlay]&&!secondaryRenderedFrame&&!statsFrameClock.active);
 std::cout<<"PASS production secondary statistics: primary pause/Tab/foreground independence, one sampler, sparse-frame edge anchors, permyriad drag bounds, visible grip, movement without rerender, menu/round/process invalidation and hotplug recovery\n";
}
'''
with tempfile.TemporaryDirectory(prefix='secondary-stats-') as directory:
 temp=Path(directory);cpp=temp/'main.cpp';exe=temp/'test'
 cpp.write_text(prefix+render_structs+host+placement+drag_host+
                function('static void paintSecondaryDragGrip(UnitSurface&surface)')+
                function('static void resetSecondary(){')+
                function('static bool updateSecondary(DWORD now,bool battlefield,bool displayFrame)')+
                post+function('static void tick(bool displayFrame=false)')+scenario)
 subprocess.run(['g++','-std=c++17','-O2','-Wall','-Wextra','-Wno-misleading-indentation','-Wno-missing-field-initializers',str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
