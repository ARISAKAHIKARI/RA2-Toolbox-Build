"""Execute production stacking and minimap lifecycle with Win32 boundary doubles.

The model preserves visible HWNDs while promoting the game above their entire
topmost group. This checks recovery decisions and ordering, not DWM composition,
DirectDraw device loss or a real Alt-Tab session. The real HWND counterpart is
overlay_stack_windows.cpp.
"""
from pathlib import Path
import argparse
import json
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--stack-header', type=Path, default=root / 'shared/source/overlay_stack.h')
parser.add_argument('--minimap-header', type=Path, default=root / 'assist/source/minimap_overlay.h')
parser.add_argument('--only', choices=('stack', 'minimap', 'all'), default='all')
args = parser.parse_args()

def function(text, signature):
    start = text.index(signature)
    body = text.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

windows = r'''
#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <map>
#include <vector>
using HDC=void*;using BOOL=int;using DWORD=uint32_t;using UINT=unsigned;using LONG=long;using HWND=void*;using HINSTANCE=void*;using WPARAM=uintptr_t;using LPARAM=intptr_t;using LRESULT=intptr_t;
constexpr BOOL FALSE=0,TRUE=1;constexpr int SW_HIDE=0,GA_ROOT=2,GW_HWNDNEXT=2,GW_HWNDPREV=3,GWL_EXSTYLE=-20;
constexpr UINT SWP_NOSIZE=1,SWP_NOMOVE=2,SWP_NOZORDER=4,SWP_NOREDRAW=8,SWP_NOACTIVATE=0x10,SWP_SHOWWINDOW=0x40,SWP_NOCOPYBITS=0x100,SWP_NOOWNERZORDER=0x200,SWP_NOSENDCHANGING=0x400;
constexpr int WS_EX_TOOLWINDOW=1,WS_EX_NOACTIVATE=2,WS_EX_TOPMOST=4,WS_EX_LAYERED=8,WS_POPUP=1,LWA_ALPHA=2;
#define HWND_TOPMOST reinterpret_cast<HWND>(intptr_t(-1))
struct POINT{LONG x=0,y=0;};struct RECT{LONG left=0,top=0,right=0,bottom=0;};
struct WNDCLASSW{LRESULT(*lpfnWndProc)(HWND,UINT,WPARAM,LPARAM)=nullptr;HINSTANCE hInstance=nullptr;const wchar_t*lpszClassName=nullptr;void*hCursor=nullptr;};
struct Window{bool visible=false;DWORD pid=1;RECT rect{};int places=0,paints=0;DWORD thread=1;bool topmost=false;};
inline std::map<HWND,Window> windows;inline std::vector<HWND> order;inline uintptr_t serial=100;inline HWND foreground=nullptr;
inline DWORD clockValue=1000;inline int batches=0,rejectedBatches=0;inline bool failBatch=false;
inline HWND makeWindow(DWORD pid=1,DWORD thread=1){HWND h=reinterpret_cast<HWND>(++serial);windows[h].pid=pid;windows[h].thread=thread;order.insert(order.begin(),h);return h;}
inline BOOL IsWindow(HWND h){return windows.count(h);}inline BOOL IsWindowVisible(HWND h){return IsWindow(h)&&windows[h].visible;}
inline DWORD GetCurrentProcessId(){return 1;}inline DWORD GetWindowThreadProcessId(HWND h,DWORD*p){if(p)*p=IsWindow(h)?windows[h].pid:0;return IsWindow(h)?windows[h].thread:0;}
inline DWORD GetTickCount(){return clockValue;}inline HWND GetForegroundWindow(){return foreground;}inline HWND GetAncestor(HWND h,UINT){return h;}
inline HWND GetTopWindow(HWND){return order.empty()?nullptr:order.front();}
inline HWND GetWindow(HWND h,UINT what){auto i=std::find(order.begin(),order.end(),h);if(i==order.end())return nullptr;if(what==GW_HWNDPREV)return i==order.begin()?nullptr:*--i;assert(what==GW_HWNDNEXT);return ++i!=order.end()?*i:nullptr;}
inline intptr_t GetWindowLongPtrW(HWND h,int field){assert(field==GWL_EXSTYLE);return IsWindow(h)&&windows[h].topmost?WS_EX_TOPMOST:0;}
inline BOOL GetClientRect(HWND h,RECT*r){if(!IsWindow(h))return FALSE;*r={0,0,windows[h].rect.right-windows[h].rect.left,windows[h].rect.bottom-windows[h].rect.top};return TRUE;}
inline BOOL GetWindowRect(HWND h,RECT*r){if(!IsWindow(h))return FALSE;*r=windows[h].rect;return TRUE;}
inline BOOL ClientToScreen(HWND h,POINT*p){if(!IsWindow(h))return FALSE;p->x+=windows[h].rect.left;p->y+=windows[h].rect.top;return TRUE;}
inline BOOL SetWindowPos(HWND h,HWND after,int x,int y,int width,int height,UINT flags){
 if(!IsWindow(h))return FALSE;assert(flags&SWP_NOACTIVATE);auto&w=windows[h];++w.places;
 if(!(flags&SWP_NOMOVE)){w.rect.right+=x-w.rect.left;w.rect.bottom+=y-w.rect.top;w.rect.left=x;w.rect.top=y;}
 if(!(flags&SWP_NOSIZE)){w.rect.right=w.rect.left+width;w.rect.bottom=w.rect.top+height;}
 if(flags&SWP_SHOWWINDOW)w.visible=true;
 if(!(flags&SWP_NOZORDER)){order.erase(std::remove(order.begin(),order.end(),h),order.end());if(after==HWND_TOPMOST){w.topmost=true;order.insert(order.begin(),h);}else{auto at=std::find(order.begin(),order.end(),after);assert(at!=order.end());w.topmost=windows[after].topmost;order.insert(++at,h);}}
 return TRUE;
}
struct Deferred{struct Move{HWND window,after;int x,y,w,h;UINT flags;};std::vector<Move> moves;};using HDWP=Deferred*;
inline HDWP BeginDeferWindowPos(int){return failBatch?nullptr:new Deferred;}
inline HDWP DeferWindowPos(HDWP batch,HWND w,HWND after,int x,int y,int width,int height,UINT flags){
 assert(batch&&!(flags&SWP_SHOWWINDOW));batch->moves.push_back({w,after,x,y,width,height,flags});return batch;
}
inline BOOL EndDeferWindowPos(HDWP batch){
 // Real Windows also rejected the single-owner batch in run 40157687203.
 // No group repair may depend on deferred success, regardless of owner IDs.
 ++batches;++rejectedBatches;delete batch;return FALSE;
}
inline void ShowWindow(HWND h,int command){assert(command==SW_HIDE);windows[h].visible=false;}
inline HINSTANCE GetModuleHandleW(void*){return reinterpret_cast<HINSTANCE>(1);}inline BOOL RegisterClassW(WNDCLASSW*){return TRUE;}
inline HWND CreateWindowExW(int,const wchar_t*,const wchar_t*,int,int,int,int,int,HWND,void*,HINSTANCE,void*){return makeWindow();}
inline void SetLayeredWindowAttributes(HWND,int,int,int){}inline BOOL InvalidateRect(HWND h,void*,BOOL){++windows[h].paints;return TRUE;}inline void (*updateHook)(HWND)=nullptr;inline void UpdateWindow(HWND h){if(updateHook)updateHook(h);}inline HDC GetDC(HWND h){return h;}inline int ReleaseDC(HWND,HDC){return 1;}
'''

prefix = r'''
#include <iostream>
#include <memory>
#include <cmath>
#include @STACK_HEADER@
#include @GATE_HEADER@
#include @SECONDARY_LAYOUT_HEADER@
#include "../../shared/source/overlay_recovery.h"
#include "../source/radar_layout.h"
static bool higher(HWND a,HWND b){for(HWND w:GetTopWindow(nullptr)?order:std::vector<HWND>{}){if(w==a)return true;if(w==b)return false;}return false;}
static HWND game,ui;static DWORD pid=2;static bool focused=true,enabled=true,minimapEnabled=true,minimapSecondary=false,secondAvailable=false,battlefieldVisible=true;
static int minimapMode=0,minimapScale=10,minimapPosition=1,minimapX=0,minimapY=0,minimapAlignX=0,minimapAlignY=0,fps=60;
static int minimapSecondaryX=10000,minimapSecondaryY=10000,minimapSecondaryWidth=320;
static bool renderVulkanEnabled=false;static int effectiveInfoFPS=144;
static int informationFPS(){return effectiveInfoFPS;}
static struct{int width=800,height=600,renderStatus=1,chat=0,cameraX=0,cameraY=0;uint32_t match=1;}snap;
static struct{uint32_t renderReveal=0,mapStrategy=0;}extra;static radarlayout::Box aircraftButtonBox;
static bool foregroundGame(){return focused;}static uint32_t commandEpoch=1;static uint32_t commandMatch(){return commandEpoch;}static uint32_t displayMatch(){return snap.match;}
static struct{int reads=0;bool readFresh(uint32_t,void*,size_t){++reads;return true;}}externalReader;
using HMONITOR=void*;static RECT secondWork{-1920,-100,0,940};
namespace secondarydisplay {struct Target{HMONITOR monitor=nullptr;RECT work{};bool available()const{return monitor&&work.right>work.left&&work.bottom>work.top;}};struct Monitor{void invalidate(){}Target get(HWND,HWND,DWORD){return secondAvailable?Target{reinterpret_cast<void*>(2),secondWork}:Target{};}};}
namespace external{namespace display{struct State{bool visible()const{return battlefieldVisible;}};template<class F>State inspect(F){return {};}}namespace mapbounds{struct NativeViewport{double width=0,height=0;};}namespace radar{
struct Geometry{int offsetX=0,offsetY=0;double rawWidth=10,rawHeight=10;bool same(const Geometry&o)const{return rawWidth==o.rawWidth&&rawHeight==o.rawHeight&&offsetX==o.offsetX&&offsetY==o.offsetY;}bool valid()const{return true;}mapbounds::NativeViewport nativeViewport()const{return {rawWidth*30,rawHeight*15};}};struct Terrain{Geometry geo;bool reveal=false;};struct Scene{int width=2,height=2;std::vector<uint32_t>pixels=std::vector<uint32_t>(4,0xff123456);};struct Marker{double u=0,v=0;uint32_t color=0;bool building=false;};struct Frame{std::shared_ptr<Terrain>terrain=std::make_shared<Terrain>();std::shared_ptr<Scene>scene,map=std::make_shared<Scene>();std::vector<Marker>markers;};
}}
namespace radarpanel{inline bool failStage=false,failPresent=false;struct Buffer{bool painted=false,valid=false,pending=false;int w=0,h=0,pw=0,ph=0,stages=0;bool ready(int width,int height)const{return valid&&width==w&&height==h;}template<class Image>bool stage(HDC,const Image&,const radargate::Visual&v){++stages;pending=!failStage;pw=v.width;ph=v.height;return pending;}bool promote(){if(!pending)return false;w=pw;h=ph;pending=false;valid=true;return true;}bool present(HDC,int width,int height){painted=ready(width,height)&&!failPresent;return painted;}};}
namespace minimap{
struct Request{DWORD pid;uint32_t match;int rate;uint32_t reveal;int mode,width,height;bool gpu=false;};
struct Worker{std::shared_ptr<const external::radar::Frame>latest=std::make_shared<external::radar::Frame>();bool paused=false;int rate=0;void set(Request r){paused=false;rate=r.rate;}void pause(){paused=true;}auto get(){return latest;}};
static Worker worker;static HWND window,secondaryWindow;static secondarydisplay::Monitor secondaryMonitor;static std::shared_ptr<const external::radar::Frame>frame;static external::radar::Geometry lastGeometry;
static radargate::PaintGate<external::radar::Frame> primaryPaintGate,secondaryPaintGate;
@MINIMAP_WORLD@
enum class SecondaryEdit{None,Move,Resize,ResizeUpperLeft};static SecondaryEdit secondaryEdit=SecondaryEdit::None;
static void finishSecondaryEdit(){secondaryEdit=SecondaryEdit::None;}
static bool dragging=false;static uint32_t dragMatch=0;static HWND dragWindow;static overlayui::Recovery recovery,secondaryRecovery;
static radarpanel::Buffer paintBuffer,secondaryPaintBuffer;
static void cancelDrag(){dragging=false;dragMatch=0;}
static LRESULT proc(HWND w,UINT,WPARAM,LPARAM){RECT r{};GetClientRect(w,&r);(w==secondaryWindow?secondaryPaintBuffer:paintBuffer).present(w,r.right,r.bottom);return 0;}
@MINIMAP_PRESENTATION@
@MINIMAP_PRIMARY@
@MINIMAP_SECONDARY@
@MINIMAP_HIDE@
@MINIMAP_PLACE@
@MINIMAP_UPDATE@
}
'''

scenario = r'''
static void stackCase(DWORD statsThread){
 using namespace overlaystack;game=makeWindow(2);windows[game]={true,2,{0,0,800,600}};foreground=game;
 HWND scene=makeWindow(),world=makeWindow(),stats=makeWindow(1,statsThread),map=makeWindow(),watch=makeWindow(),cursor=makeWindow();
 int initialBatches=batches,initialRejected=rejectedBatches;
 auto placeOne=[&](HWND w,Layer l){assert(place(w,l,10,20,200,100,SWP_SHOWWINDOW));};
 placeOne(world,Layer::World);placeOne(map,Layer::Minimap);placeOne(cursor,Layer::Cursor);placeOne(stats,Layer::Statistics);placeOne(watch,Layer::Watch);placeOne(scene,Layer::RenderUpgrade);
 auto ordered=[&]{return higher(cursor,watch)&&higher(watch,map)&&higher(map,stats)&&higher(stats,world)&&higher(world,scene);};assert(ordered());
 // Fullscreen focus return may raise the game without clearing WS_VISIBLE.
 clockValue+=500;SetWindowPos(game,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);assert(higher(game,cursor));
 placeOne(stats,Layer::Statistics);
 for(auto w:{scene,world,stats,map,watch,cursor})assert(IsWindowVisible(w)&&higher(w,game)&&"repair must lift the whole visible overlay group above the promoted game");
 assert(ordered());int before=batches,statsPlaces=windows[stats].places;std::vector<int> otherPlaces;
 for(auto w:{scene,world,map,watch,cursor})otherPlaces.push_back(windows[w].places);
 for(int i=0;i<100;++i)placeOne(stats,Layer::Statistics);assert(batches==before&&windows[stats].places==statsPlaces&&"unchanged geometry/visibility/actual anchor must not send position callbacks");
 size_t other=0;for(auto w:{scene,world,map,watch,cursor})assert(windows[w].places==otherPlaces[other++]);
 // A repeat promotion inside the old250ms throttle must recover now, including
 // the case where the cursor first repairs only itself above the game.
 clockValue+=1;SetWindowPos(game,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);assert(higher(game,cursor));
 assert(keepAbove(game)&&ordered()&&higher(scene,game));
 clockValue+=1;SetWindowPos(game,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);placeOne(cursor,Layer::Cursor);assert(ordered()&&higher(scene,game));
 // Repeated game promotions recover without moving the pointer or pressing Tab.
 for(int n=0;n<5;++n){clockValue+=1000;SetWindowPos(game,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);placeOne(watch,Layer::Watch);assert(ordered()&&higher(world,game));}
 ShowWindow(map,SW_HIDE);clockValue+=1000;SetWindowPos(game,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);placeOne(stats,Layer::Statistics);
 assert(!IsWindowVisible(map)&&higher(world,scene)&&higher(scene,game)&&higher(cursor,watch)&&higher(watch,stats)&&higher(stats,world));
 // No caller requests a hidden panel; a group repair must never show it.
 auto hiddenPlaces=windows[map].places;placeOne(world,Layer::World);assert(!IsWindowVisible(map)&&windows[map].places==hiddenPlaces);
 assert(rejectedBatches==initialRejected&&batches==initialBatches&&"all owner groups must avoid the rejected deferred API path");
 std::cout<<"PASS group keepalive ("<<(statsThread==1?"single-owner ordered chain":"mixed-owner ordered chain")<<"): promoted game, stable layer order, repeated focus returns, throttled per-window calls, hidden panels stay hidden"<<std::endl;
 for(auto w:{scene,world,stats,map,watch,cursor}){forget(w);windows.erase(w);order.erase(std::remove(order.begin(),order.end(),w),order.end());}
}
static void minimapCase(){
 game=makeWindow(2);windows[game]={true,2,{0,0,800,600}};foreground=game;clockValue+=2000;
 updateHook=[](HWND w){minimap::proc(w,0,0,0);};POINT origin{};RECT client{0,0,800,600};minimap::update(origin,client,battlefieldVisible);HWND window=minimap::window;assert(IsWindowVisible(window)&&externalReader.reads==0&&"minimap must reuse the owner display gate");
 int paints=windows[window].paints;clockValue+=25;minimap::update(origin,client,battlefieldVisible);
 assert(windows[window].paints==paints&&"an unchanged map/camera should not repaint");
 snap.cameraX+=30;minimap::update(origin,client,battlefieldVisible);
 assert(windows[window].paints==paints+1&&"camera motion repaints once");
 assert(minimap::worker.rate==144&&fps==60);effectiveInfoFPS=240;
 int stages=minimap::paintBuffer.stages;int placed=windows[window].places;clockValue+=1000;minimap::update(origin,client,battlefieldVisible);assert(minimap::worker.rate==240);
 assert(windows[window].places>placed&&"unchanged visible minimap must periodically repair its z-order");
 assert(minimap::paintBuffer.stages==stages&&"periodic recovery must copy the completed front without redoing HALFTONE/markers");
 auto prior=minimap::worker.latest;minimap::worker.latest=std::make_shared<external::radar::Frame>(*prior);paints=windows[window].paints;
 minimap::update(origin,client,battlefieldVisible);assert(windows[window].paints==paints&&minimap::paintBuffer.stages==stages&&"a new observation wrapper sharing the same pixels does not repaint");
 auto changed=std::make_shared<external::radar::Frame>(*prior);changed->map=std::make_shared<external::radar::Scene>();minimap::worker.latest=changed;radarpanel::failStage=true;
 minimap::update(origin,client,battlefieldVisible);assert(IsWindowVisible(window)&&windows[window].paints==paints&&"failed preparation retains the visible complete panel");radarpanel::failStage=false;
 minimap::update(origin,client,battlefieldVisible);assert(windows[window].paints==paints+1&&"failed preparation did not close the repaint gate");
 changed=std::make_shared<external::radar::Frame>(*changed);changed->map=std::make_shared<external::radar::Scene>();minimap::worker.latest=changed;radarpanel::failPresent=true;minimap::update(origin,client,battlefieldVisible);paints=windows[window].paints;
 radarpanel::failPresent=false;minimap::update(origin,client,battlefieldVisible);assert(windows[window].paints==paints+1&&"a failed WM_PAINT presentation is retried before periodic recovery");
 // An incompatible world must never retain the previous reveal policy.
 ++snap.match;radarpanel::failStage=true;minimap::update(origin,client,battlefieldVisible);assert(!IsWindowVisible(window));radarpanel::failStage=false;
 minimap::update(origin,client,battlefieldVisible);assert(IsWindowVisible(window));
 SetWindowPos(game,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);clockValue+=1000;minimap::update(origin,client,battlefieldVisible);assert(higher(window,game));
 for(int phase=0;phase<4;++phase){
  focused=phase!=0;minimapEnabled=phase!=1;snap.chat=phase==2;snap.renderStatus=phase==3?0:1;
  minimap::update(origin,client,battlefieldVisible);assert(!IsWindowVisible(window)&&minimap::worker.paused);placed=windows[window].places;
  clockValue+=3000;minimap::update(origin,client,battlefieldVisible);assert(!IsWindowVisible(window)&&windows[window].places==placed);
  focused=minimapEnabled=true;snap.chat=0;snap.renderStatus=1;minimap::update(origin,client,battlefieldVisible);assert(IsWindowVisible(window)&&higher(window,game));
 }
 // A single published frame drives both windows. The secondary copy remains
 // eligible when the game client is unavailable, foreground lost, chat shown,
 // primary hidden, or the assist module paused. No screen means no worker.
 minimapSecondary=secondAvailable=true;minimapEnabled=enabled=focused=false;snap.chat=1;
 auto sharedFrame=minimap::worker.latest;minimap::update({},{},battlefieldVisible);assert(!IsWindowVisible(window)&&IsWindowVisible(minimap::secondaryWindow)&&!minimap::worker.paused&&minimap::frame==sharedFrame);
 auto second=minimap::secondaryWindow;auto rectangle=windows[second].rect;assert(rectangle.left>=secondWork.left&&rectangle.right<=secondWork.right&&rectangle.top>=secondWork.top&&rectangle.bottom<=secondWork.bottom);
 paints=windows[second].paints;clockValue+=25;minimap::update({},{},battlefieldVisible);
 assert(windows[second].paints==paints&&"unchanged secondary map should not repaint");
 snap.cameraY+=30;minimap::update({},{},battlefieldVisible);assert(windows[second].paints==paints+1);
 ++commandEpoch;minimap::update({},{},battlefieldVisible);assert(minimap::frame==sharedFrame);
 secondAvailable=false;minimap::update({},{},battlefieldVisible);assert(!IsWindowVisible(second)&&minimap::worker.paused);
 secondAvailable=true;minimap::update({},{},battlefieldVisible);assert(IsWindowVisible(second)&&!minimap::worker.paused);
 battlefieldVisible=false;minimap::update({},{},battlefieldVisible);assert(!IsWindowVisible(second)&&minimap::worker.paused);battlefieldVisible=true;
 std::cout<<"PASS production minimap lifecycle: unchanged-geometry keepalive, game promotion, focus/disable/chat/non-battlefield hiding and automatic eligible return without Tab\n";
}
int main(){@CASES@}
'''

minimap = args.minimap_header.read_text()
code = prefix.replace('@STACK_HEADER@', json.dumps(str(args.stack_header.resolve())))
code = code.replace('@GATE_HEADER@', json.dumps(str((root / 'assist/source/radar_paint_gate.h').resolve())))
code = code.replace('@SECONDARY_LAYOUT_HEADER@', json.dumps(str((root / 'assist/source/secondary_radar_layout.h').resolve())))
world=minimap.split('struct PresentedWorld {',1)[1].split('static PresentedWorld primaryWorld,secondaryWorld;',1)[0]
code=code.replace('@MINIMAP_WORLD@','struct PresentedWorld {'+world+'static PresentedWorld primaryWorld,secondaryWorld;')
code = code.replace('@MINIMAP_PRIMARY@', function(minimap, 'static void hidePrimary()'))
code = code.replace('@MINIMAP_SECONDARY@', function(minimap, 'static void hideSecondary()'))
code = code.replace('@MINIMAP_PRESENTATION@', function(minimap, 'static bool presentation(int width,int height,std::shared_ptr<const external::radar::Scene>&image,radargate::Visual&visual)'))
code = code.replace('@MINIMAP_PLACE@', function(minimap, 'static void placePanel(bool second,int x,int y,int width,int height)'))
code = code.replace('@MINIMAP_HIDE@', function(minimap, 'static void hide()'))
code = code.replace('@MINIMAP_UPDATE@', function(minimap, 'static void update(POINT origin,const RECT&client,bool verifiedBattlefield)'))
cases = {'all': 'stackCase(1);stackCase(2);minimapCase();', 'stack': 'stackCase(1);stackCase(2);', 'minimap': 'minimapCase();'}[args.only]
with tempfile.TemporaryDirectory() as tmp:
    temp = Path(tmp)
    (temp / 'windows.h').write_text(windows)
    cpp = temp / 'overlay_keepalive.cpp'
    exe = temp / 'overlay_keepalive'
    cpp.write_text(code + scenario.replace('@CASES@', cases))
    subprocess.run(['g++', '-std=c++17', '-O2', '-pthread', '-I' + str(temp),
                    '-I' + str(root / 'assist/tests'), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=20)
