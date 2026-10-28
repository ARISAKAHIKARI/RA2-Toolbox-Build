"""Execute production tracking-window input/lifecycle with Win32 boundary doubles.
Does not prove Windows composition/cursor behavior or a real game camera.
"""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
prefix=(root/'assist/tests/watch_host.cpp').read_text().split('int main(){',1)[0]
prefix=prefix.replace('int mapStrategy=0;', 'int mapStrategy=0,watchSecondaryX=10000,watchSecondaryY=0;')
prefix=prefix.replace('static bool foregroundGame(){return true;}','static bool focused=true;static bool foregroundGame(){return focused;}')
prefix=prefix.replace('struct Graphics {','struct Graphics {Graphics(void* =nullptr){}void ScaleTransform(float,float){}')
prefix=prefix.replace('static struct {bool read(uint32_t p,void*out,size_t n){','static struct {bool readFresh(uint32_t p,void*out,size_t n){return read(p,out,n);}bool read(uint32_t p,void*out,size_t n){')
prefix=prefix.replace('static uint32_t blockedAddress=0,', 'static uint32_t displayGateReads=0;static uint32_t blockedAddress=0,')
prefix=prefix.replace('bool readFresh(uint32_t p,void*out,size_t n){if(p==0xA8EB44', 'bool readFresh(uint32_t p,void*out,size_t n){if(p==0xA8E378)++displayGateReads;if(p==0xA8EB44')
prefix+=r'''
// Execute the production async host as well as the window handlers. Only the
// external memory/assets and Win32/GDI boundaries are replaced here.
#include <mutex>
#include <condition_variable>
#include <chrono>
namespace watchtest {
static std::mutex gate;static std::condition_variable changed;static bool held=false,entered=false;
static bool freshHeld=false,freshEntered=false;
static void hold(){std::lock_guard<std::mutex> lock(gate);held=true;entered=false;}
static void release(){std::lock_guard<std::mutex> lock(gate);held=false;changed.notify_all();}
static bool awaitEntry(){std::unique_lock<std::mutex> lock(gate);return changed.wait_for(lock,std::chrono::seconds(5),[]{return entered;});}
static void open(){std::unique_lock<std::mutex> lock(gate);if(held){entered=true;changed.notify_all();changed.wait(lock,[]{return !held;});}}
static void holdFresh(){std::lock_guard<std::mutex> lock(gate);freshHeld=true;freshEntered=false;}
static void releaseFresh(){std::lock_guard<std::mutex> lock(gate);freshHeld=false;changed.notify_all();}
static bool awaitFresh(){std::unique_lock<std::mutex> lock(gate);return changed.wait_for(lock,std::chrono::seconds(5),[]{return freshEntered;});}
static void fresh(){std::unique_lock<std::mutex> lock(gate);if(freshHeld){freshEntered=true;changed.notify_all();changed.wait(lock,[]{return !freshHeld;});}}
}
namespace external {
struct ReadSession {
 std::array<int32_t,6> bounds{};bool haveBounds=false;
 bool open(uint32_t){watchtest::open();return true;}void close(){}void begin(){haveBounds=false;}
 bool read(uint32_t p,void*out,size_t n){
  if(p==mapbounds::Map+0xF4&&n==sizeof bounds){if(!haveBounds){if(!externalReader.read(p,bounds.data(),n))return false;haveBounds=true;}memcpy(out,bounds.data(),n);return true;}
  return externalReader.read(p,out,n);
 }
 bool readFresh(uint32_t p,void*out,size_t n){if(p==mapbounds::Map+0xF4&&n==sizeof bounds)watchtest::fresh();return externalReader.readFresh(p,out,n);}
};
struct SharedReadView {ReadSession*reader;bool operator()(uint32_t p,void*out,size_t n)const{return reader->read(p,out,n);}};
namespace cameraassets {template<class Reader>struct Context:Backend {
 explicit Context(Reader&){}struct Files{size_t count(){return 1;}}files;
 std::string text(uint32_t p,int n){return gameassets::text(p,n);}
};}
}
#include "../source/watch_host.h"
'''
shim=r'''
#include "../../shared/source/overlay_recovery.h"
using namespace assist;
using HWND=void*;using HDC=void*;using HBITMAP=void*;using HGDIOBJ=void*;using LONG=long;using LONG_PTR=intptr_t;using UINT=unsigned;using WPARAM=uintptr_t;using LPARAM=intptr_t;using LRESULT=intptr_t;
#define CALLBACK
constexpr int FALSE=0,TRUE=1,SW_HIDE=0,GWLP_USERDATA=1,HTCLIENT=1,MA_NOACTIVATE=3,SRCCOPY=1;
constexpr UINT WM_NCCREATE=0x81,WM_NCHITTEST=0x84,WM_MOUSEACTIVATE=0x21,WM_SETCURSOR=0x20,WM_LBUTTONDOWN=0x201,WM_RBUTTONDOWN=0x204,WM_LBUTTONUP=0x202,WM_RBUTTONUP=0x205,WM_LBUTTONDBLCLK=0x203,WM_RBUTTONDBLCLK=0x206,WM_MOUSEMOVE=0x200,WM_ERASEBKGND=0x14,WM_PAINT=0xf,WM_CAPTURECHANGED=0x215,WM_CANCELMODE=0x1f;
constexpr int MK_SHIFT=4,MK_LBUTTON=1,WS_EX_TOOLWINDOW=1,WS_EX_NOACTIVATE=2,WS_EX_TOPMOST=4,WS_EX_LAYERED=8,WS_POPUP=1,LWA_ALPHA=2,SWP_NOACTIVATE=1,SWP_SHOWWINDOW=2,SWP_NOSIZE=4,SWP_NOZORDER=8;
#define HWND_TOPMOST reinterpret_cast<HWND>(1)
#define HGDI_ERROR reinterpret_cast<HGDIOBJ>(-1)

struct POINT{int x,y;};struct RECT{int left=0,top=0,right=0,bottom=0;};struct PAINTSTRUCT{};struct CREATESTRUCTW{void*lpCreateParams;};
static unsigned HIWORD(LPARAM value){return (unsigned(value)>>16)&0xffffu;}
using Proc=LRESULT(*)(HWND,UINT,WPARAM,LPARAM);struct WNDCLASSW{Proc lpfnWndProc;int hInstance;const wchar_t*lpszClassName;};
struct Window {RECT rect;LONG_PTR data=0;bool visible=false;int raises=0,invalidations=0;};static std::map<HWND,Window>win;static HWND game=reinterpret_cast<HWND>(7),ui=reinterpret_cast<HWND>(8);static uintptr_t nextWindow=20;static Proc windowProc;static bool fresh=true;static int wakes=0,cursors=0;
static void battlefield(bool active){memory[0xA8E378]=active;}
static int settingsWrites=0,syncedPositions=0;
static void changedSetting(){++settingsWrites;}
static void syncSecondaryLayoutControls(){++syncedPositions;}
static struct {volatile LONG cameraSeq=0;uint32_t cameraMatch=0,cameraTick=0;int cameraTargetX=0,cameraTargetY=0;}mailbox;static auto*shared=&mailbox;
static bool commandFresh(uint32_t){return fresh;}static uint32_t sampleClock(decltype(shared)){return clockValue;}static void wakeGame(){++wakes;}static void MemoryBarrier(){}static void InterlockedIncrement(volatile LONG*p){++*p;}
static HDC CreateCompatibleDC(HDC){return reinterpret_cast<HDC>(1);}static HBITMAP CreateCompatibleBitmap(HDC,int,int){return reinterpret_cast<HBITMAP>(2);}static HGDIOBJ SelectObject(HDC,HGDIOBJ){return reinterpret_cast<HGDIOBJ>(3);}static void DeleteObject(HGDIOBJ){}static void DeleteDC(HDC){}static bool BitBlt(HDC,int,int,int,int,HDC,int,int,int){return true;}
static HWND captured=nullptr;static POINT cursor{};static bool GetCursorPos(POINT*p){*p=cursor;return true;}
static HWND GetCapture(){return captured;}static HWND SetCapture(HWND w){auto prior=captured;captured=w;return prior;}static bool ReleaseCapture(){captured=nullptr;return true;}
static void ShowWindow(HWND w,int){win[w].visible=false;}static void DestroyWindow(HWND w){win.erase(w);}static void*lastCursor=nullptr;static void SetCursor(void*p){lastCursor=p;++cursors;}static constexpr int IDC_ARROW=32512,IDC_SIZEALL=32646;static void*LoadCursorW(void*,int name){return reinterpret_cast<void*>(name==IDC_SIZEALL?98:99);}static LONG_PTR SetWindowLongPtrW(HWND w,int,LONG_PTR data){return win[w].data=data;}static LONG_PTR GetWindowLongPtrW(HWND w,int){return win[w].data;}
static LRESULT DefWindowProcW(HWND,UINT,WPARAM,LPARAM){return 0;}static HDC BeginPaint(HWND,PAINTSTRUCT*){return reinterpret_cast<HDC>(1);}static void EndPaint(HWND,PAINTSTRUCT*){}static void GetClientRect(HWND w,RECT*r){*r={0,0,win[w].rect.right-win[w].rect.left,win[w].rect.bottom-win[w].rect.top};}
static bool ScreenToClient(HWND w,POINT*p){p->x-=win[w].rect.left;p->y-=win[w].rect.top;return true;}
static int GetModuleHandleW(void*){return 1;}static void RegisterClassW(WNDCLASSW*c){windowProc=c->lpfnWndProc;}
static HWND CreateWindowExW(int,const wchar_t*,const wchar_t*,int,int,int,int,int,void*,void*,int,void*data){HWND w=reinterpret_cast<HWND>(nextWindow++);CREATESTRUCTW c{data};windowProc(w,WM_NCCREATE,0,reinterpret_cast<LPARAM>(&c));return w;}
static void SetLayeredWindowAttributes(HWND,int,int,int){}static bool GetWindowRect(HWND w,RECT*r){*r=win[w].rect;return true;}static bool IsWindowVisible(HWND w){return win[w].visible;}
static bool SetWindowPos(HWND w,HWND,int x,int y,int width,int height,int flags){assert(flags&SWP_NOACTIVATE);auto&v=win[w];if(flags&SWP_NOSIZE){width=v.rect.right-v.rect.left;height=v.rect.bottom-v.rect.top;}v.rect={x,y,x+width,y+height};if(flags&SWP_SHOWWINDOW)v.visible=true;if(!(flags&SWP_NOZORDER))++v.raises;return true;}
namespace overlaystack {enum class Layer{Watch};static bool place(HWND w,Layer,int x,int y,int width,int height,int flags){return SetWindowPos(w,HWND_TOPMOST,x,y,width,height,flags);}}
static void InvalidateRect(HWND w,void*,bool){++win[w].invalidations;}
static int synchronousPaints=0;static void UpdateWindow(HWND w){++synchronousPaints;windowProc(w,WM_PAINT,0,0);}
#define RA2_SECONDARY_DISPLAY_H
using HMONITOR=void*;
static RECT secondWork{-1920,-100,0,940};
namespace secondarydisplay {struct Target{HMONITOR monitor=nullptr;RECT work{};bool available()const{return monitor&&work.right>work.left&&work.bottom>work.top;}};struct Monitor{void invalidate(){}Target get(HWND,HWND,DWORD){return secondAvailable?Target{reinterpret_cast<void*>(2),secondWork}:Target{};}};}
#include "../source/watch_windows.h"
int main(){
 battlefield(true);auto put=[](uint32_t p,auto v){memcpy(memory.data()+p,&v,sizeof v);};
 uint32_t p=0x10000;put(p,0x7F5C70u);put(p+0x10,42u);put(p+0x6C,100);put(p+0x74,uint8_t(1));put(p+0x90,uint8_t(1));put(p+0x9C,40*256);put(p+0xA0,30*256);put(p+0xA4,208);put(0xB0CD48,.144337567);
 for(int i=0;i<3;++i){watchhost::tracks[i].target={p,42};watchhost::views[i].shown=true;watchhost::views[i].frame.focus={p,42};}watchhost::arrange();
 RECT client{0,0,1600,1200};POINT origin{100,200};int beforeGate=displayGateReads;watchwindows::update(origin,client,true);assert(displayGateReads==beforeGate&&"the verified display cycle must not read the same gate per watch window");assert(win.size()==3&&synchronousPaints==3);for(auto w:watchwindows::windows){assert(win[w].visible&&win[w].raises==1&&win[w].invalidations==1);}
 watchhost::views[0].pixels=std::make_shared<std::vector<uint32_t>>(640*512,0xffabcdef);drawnSources.clear();watchwindows::proc(watchwindows::windows[0],WM_PAINT,0,0);assert(drawnSources.size()==1&&drawnSources[0]==std::make_pair(640,512)&&presentedInterpolation==InterpolationModeNearestNeighbor&&presentedPixelOffset==PixelOffsetModeHalf);
 battlefield(false);beforeGate=displayGateReads;int beforeDraws=draws;watchwindows::proc(watchwindows::windows[0],WM_PAINT,0,0);assert(displayGateReads>beforeGate&&draws==beforeDraws&&"an unrelated paint must still check the live menu state");battlefield(true);
 Graphics clipping;watchwindows::occlude(clipping);assert(excluded.size()==3);for(int i=0;i<3;++i){auto b=watchhost::views[i].box;assert(excluded[i].x==b.x-1&&excluded[i].y==b.y-1&&excluded[i].w==b.w+2&&excluded[i].h==b.h+2);}
 int paints=synchronousPaints;beforeGate=displayGateReads;for(int n=0;n<100;++n){for(auto&v:watchhost::views)++v.revision;watchwindows::update(origin,client,true);}assert(synchronousPaints==paints+300&&displayGateReads==beforeGate&&"verified repaint must not reread the tactical gate for each window");
 HWND middle=watchwindows::windows[1];auto box=watchhost::views[1].box;assert(win[middle].rect.left==100+box.x*2&&win[middle].rect.top==200+box.y*2);
 assert(watchwindows::proc(middle,WM_MOUSEACTIVATE,0,0)==MA_NOACTIVATE);watchwindows::proc(middle,WM_LBUTTONDOWN,0,0);assert(wakes==1&&shared->cameraSeq==2&&shared->cameraMatch==1&&shared->cameraTargetX==300&&shared->cameraTargetY==1020);
 watchwindows::proc(middle,WM_LBUTTONUP,0,0);assert(wakes==1);watchwindows::proc(middle,WM_RBUTTONDOWN,0,0);assert(watchhost::tracks[1]);
 for(int scenario=0;scenario<4;++scenario){battlefield(scenario!=0);focused=scenario!=1;fresh=scenario!=2;snap.chat=scenario==3;watchwindows::proc(middle,WM_LBUTTONDOWN,0,0);assert(wakes==1);}battlefield(true);focused=fresh=true;snap.chat=0;
 clockValue+=999;watchwindows::update(origin,client,true);assert(win[middle].raises==1);++clockValue;watchwindows::update(origin,client,true);assert(win[middle].raises==2&&win[middle].invalidations==102);
 battlefield(false);watchwindows::update(origin,client,false);for(auto w:watchwindows::windows)assert(!win[w].visible);clockValue+=2000;watchwindows::update(origin,client,false);assert(win[middle].raises==2);battlefield(true);watchwindows::update(origin,client,true);assert(win[middle].visible&&win[middle].raises==3);
 watchwindows::proc(middle,WM_RBUTTONDOWN,MK_SHIFT,0);assert(!watchhost::tracks[1]&&!watchhost::views[1].shown&&!win[middle].visible&&watchhost::tracks[0]&&watchhost::tracks[2]);clockValue+=1000;watchwindows::update(origin,client,true);assert(!win[middle].visible);
 // A garrison already found beyond the bounded discovery batch must remain
 // navigable. Reusing its verified spatial host avoids restarting at the
 // first 64 buildings on every click.
 put(p,0x7EB058u);put(p+0x81,uint8_t(1));put(p+0x90,uint8_t(0));
 constexpr uint32_t buildingItems=0x1C0000,occupantItems=0x1D0000;uint32_t host=0;
 put(0xA8EB40+4,buildingItems);put(0xA8EB40+8,100);put(0xA8EB40+16,100);
 for(int i=0;i<100;++i){uint32_t building=0x300000+i*0x800;put(buildingItems+i*4,building);put(building,0x7E3EBCu);put(building+0x10,uint32_t(i+100));put(building+0x6C,100);put(building+0x74,uint8_t(1));put(building+0x90,uint8_t(1));if(i==70)host=building;}
 put(host+0x9C,45*256);put(host+0xA0,35*256);put(host+0x688,occupantItems);put(host+0x68C,1);put(host+0x694,1);put(occupantItems,p);
 auto&garrison=watchhost::tracks[0];garrison.spatial={host,170};garrison.located=true;garrison.spatialKind=3;
 int beforeGarrisonWakes=wakes;watchwindows::proc(watchwindows::windows[0],WM_LBUTTONDOWN,0,0);
 assert(wakes==beforeGarrisonWakes+1&&shared->cameraTargetX==300&&shared->cameraTargetY==1200);
 // Navigation's fresh lifecycle decision must be committed even though no
 // camera move is sent. A delayed worker completion uses the prior control
 // revision and cannot restore this cleared slot.
 auto beforeControl=watchhost::controls;int beforeWakes=wakes;put(p+0x6C,0);
 watchwindows::proc(watchwindows::windows[0],WM_LBUTTONDOWN,0,0);
 assert(!watchhost::tracks[0]&&!watchhost::views[0].shown&&watchhost::controls==beforeControl+1&&wakes==beforeWakes);
 watchwindows::proc(watchwindows::windows[0],WM_SETCURSOR,0,0);assert(cursors==1);
 // Secondary copies share the same immutable pixels and pins while the game
 // display, assist master, foreground and chat gates are independently off.
 put(p+0x6C,100);for(int i=0;i<3;++i){watchhost::tracks[i].target={p,42};watchhost::views[i].shown=true;watchhost::views[i].pixels=watchhost::views[0].pixels;}
 auto sharedPixels=watchhost::views[0].pixels;extra.watchEnabled=false;extra.watchSecondary=true;enabled=false;focused=false;snap.chat=1;secondAvailable=true;
 watchwindows::update({},{},true);for(auto w:watchwindows::windows)assert(!win[w].visible);
 for(auto w:watchwindows::secondaryWindows){auto r=win[w].rect;assert(win[w].visible&&r.left>=secondWork.left&&r.right<=secondWork.right&&r.top>=secondWork.top&&r.bottom<=secondWork.bottom);}
 auto first=win[watchwindows::secondaryWindows[0]].rect,last=win[watchwindows::secondaryWindows[2]].rect;
 assert(first.right==secondWork.right-12&&first.top==secondWork.top+12);
 extra.watchSecondaryX=0;extra.watchSecondaryY=10000;watchwindows::update({},{},true);
 first=win[watchwindows::secondaryWindows[0]].rect;last=win[watchwindows::secondaryWindows[2]].rect;
 assert(first.left==secondWork.left+12&&last.bottom==secondWork.bottom-12);
 for(auto w:watchwindows::secondaryWindows){auto r=win[w].rect;assert(r.left>=secondWork.left&&r.right<=secondWork.right&&r.top>=secondWork.top&&r.bottom<=secondWork.bottom);}
 extra.watchSecondaryX=50000;extra.watchSecondaryY=-5;watchwindows::update({},{},true);
 first=win[watchwindows::secondaryWindows[0]].rect;assert(first.right==secondWork.right-12&&first.top==secondWork.top+12);
 secondWork={-90,0,30,30};extra.watchSecondaryX=50;extra.watchSecondaryY=50;watchwindows::invalidateDisplays();watchwindows::update({},{},true);
 for(auto w:watchwindows::secondaryWindows){auto r=win[w].rect;assert(win[w].visible&&r.left>=secondWork.left&&r.right<=secondWork.right&&r.top>=secondWork.top&&r.bottom<=secondWork.bottom);}
 secondWork={-1920,-100,0,940};extra.watchSecondaryX=10000;extra.watchSecondaryY=0;watchwindows::invalidateDisplays();watchwindows::update({},{},true);
 first=win[watchwindows::secondaryWindows[0]].rect;cursor={first.left+8,first.top+8};
 int beforeDragWakes=wakes;watchwindows::proc(watchwindows::secondaryWindows[0],WM_LBUTTONDOWN,0,8<<16);
 assert(captured==watchwindows::secondaryWindows[0]&&watchwindows::dragging&&wakes==beforeDragWakes);
 cursor.x-=180;cursor.y+=65;watchwindows::proc(watchwindows::secondaryWindows[0],WM_MOUSEMOVE,MK_LBUTTON,0);
 first=win[watchwindows::secondaryWindows[0]].rect;last=win[watchwindows::secondaryWindows[2]].rect;
 assert(first.left<secondWork.right-12-(first.right-first.left)&&first.top>secondWork.top+12&&last.bottom<=secondWork.bottom-12);
 watchwindows::update({},{},true);assert(win[watchwindows::secondaryWindows[0]].rect.left==first.left&&win[watchwindows::secondaryWindows[0]].rect.top==first.top&&"frame update must not snap a held drag back");
 watchwindows::proc(watchwindows::secondaryWindows[0],WM_LBUTTONUP,0,0);
 assert(!watchwindows::dragging&&!captured&&extra.watchSecondaryX<10000&&extra.watchSecondaryY>0&&settingsWrites==1&&syncedPositions==1);
 first=win[watchwindows::secondaryWindows[0]].rect;
 watchwindows::update({},{},true);for(auto w:watchwindows::secondaryWindows){auto r=win[w].rect;assert(r.left>=secondWork.left&&r.right<=secondWork.right&&r.top>=secondWork.top&&r.bottom<=secondWork.bottom);}
 auto settled=win[watchwindows::secondaryWindows[0]].rect;
 assert(settled.left>=first.left-1&&settled.left<=first.left+1&&settled.top>=first.top-1&&settled.top<=first.top+1&&"release should not visibly snap the dragged group");
 watchwindows::proc(watchwindows::secondaryWindows[0],WM_LBUTTONDOWN,0,8<<16);
 watchwindows::proc(watchwindows::secondaryWindows[0],WM_LBUTTONUP,0,0);
 assert(settingsWrites==1&&"a stationary grab should not save");
 for(const auto&v:watchhost::views)assert(v.pixels==sharedPixels);
 int passiveWakes=wakes;watchwindows::proc(watchwindows::secondaryWindows[0],WM_LBUTTONDOWN,0,40<<16);assert(wakes==passiveWakes);
 auto hover=win[watchwindows::secondaryWindows[0]].rect;
 cursor={hover.left+5,hover.top+5};watchwindows::proc(watchwindows::secondaryWindows[0],WM_SETCURSOR,0,0);assert(lastCursor==reinterpret_cast<void*>(98));
 cursor={hover.left+5,hover.top+50};watchwindows::proc(watchwindows::secondaryWindows[0],WM_SETCURSOR,0,0);assert(lastCursor==reinterpret_cast<void*>(99));
 excluded.clear();watchwindows::occlude(clipping);assert(excluded.empty());
 secondAvailable=false;watchwindows::update({},{},true);for(auto w:watchwindows::secondaryWindows)assert(!win[w].visible);
 secondAvailable=true;secondWork={1920,0,3200,900};watchwindows::invalidateDisplays();watchwindows::update({},{},true);for(auto w:watchwindows::secondaryWindows)assert(win[w].visible&&win[w].rect.left>=1920);
 watchwindows::proc(watchwindows::secondaryWindows[1],WM_RBUTTONDOWN,MK_SHIFT,0);assert(!watchhost::tracks[1]&&!watchhost::views[1].shown&&!win[watchwindows::secondaryWindows[1]].visible);
 watchwindows::destroy();assert(win.empty());
 std::cout<<"PASS production window handlers: one-click native-camera mailbox, independent Shift-right-close, no activation, physical scaling, one-second reassertion, menu/focus/chat gating and destroy\n";
}
'''
if __name__=='__main__':
 with tempfile.TemporaryDirectory() as tmp:
  cpp=Path(tmp)/'watch.cpp';exe=Path(tmp)/'watch';cpp.write_text(prefix+shim)
  subprocess.run(['g++','-std=c++17','-O1','-pthread','-I'+str(root/'assist/tests'),str(cpp),str(root/'third_party/xbrz/xbrz.cpp'),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)

