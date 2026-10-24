"""Execute production tracking-window input/lifecycle with Win32 boundary doubles.
Does not prove Windows composition/cursor behavior or a real game camera.
"""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
prefix=(root/'assist/tests/watch_host.cpp').read_text().split('int main(){',1)[0]
prefix=prefix.replace('static bool foregroundGame(){return true;}','static bool focused=true;static bool foregroundGame(){return focused;}')
prefix=prefix.replace('struct Graphics {','struct Graphics {Graphics(void* =nullptr){}void ScaleTransform(float,float){}')
prefix=prefix.replace('static struct {bool read(uint32_t p,void*out,size_t n){','static struct {bool readFresh(uint32_t p,void*out,size_t n){return read(p,out,n);}bool read(uint32_t p,void*out,size_t n){')
shim=r'''
#include "../../shared/source/overlay_recovery.h"
using namespace assist;
using HWND=void*;using HDC=void*;using HBITMAP=void*;using HGDIOBJ=void*;using LONG=long;using LONG_PTR=intptr_t;using UINT=unsigned;using WPARAM=uintptr_t;using LPARAM=intptr_t;using LRESULT=intptr_t;
#define CALLBACK
constexpr int FALSE=0,TRUE=1,SW_HIDE=0,GWLP_USERDATA=1,HTCLIENT=1,MA_NOACTIVATE=3,SRCCOPY=1;
constexpr UINT WM_NCCREATE=0x81,WM_NCHITTEST=0x84,WM_MOUSEACTIVATE=0x21,WM_SETCURSOR=0x20,WM_LBUTTONDOWN=0x201,WM_RBUTTONDOWN=0x204,WM_LBUTTONUP=0x202,WM_RBUTTONUP=0x205,WM_LBUTTONDBLCLK=0x203,WM_RBUTTONDBLCLK=0x206,WM_MOUSEMOVE=0x200,WM_ERASEBKGND=0x14,WM_PAINT=0xf;
constexpr int MK_SHIFT=4,WS_EX_TOOLWINDOW=1,WS_EX_NOACTIVATE=2,WS_EX_TOPMOST=4,WS_EX_LAYERED=8,WS_POPUP=1,LWA_ALPHA=2,SWP_NOACTIVATE=1,SWP_SHOWWINDOW=2;
#define HWND_TOPMOST reinterpret_cast<HWND>(1)
#define HGDI_ERROR reinterpret_cast<HGDIOBJ>(-1)

struct POINT{int x,y;};struct RECT{int left=0,top=0,right=0,bottom=0;};struct PAINTSTRUCT{};struct CREATESTRUCTW{void*lpCreateParams;};
using Proc=LRESULT(*)(HWND,UINT,WPARAM,LPARAM);struct WNDCLASSW{Proc lpfnWndProc;int hInstance;const wchar_t*lpszClassName;};
struct Window {RECT rect;LONG_PTR data=0;bool visible=false;int raises=0,invalidations=0;};static std::map<HWND,Window>win;static HWND game=reinterpret_cast<HWND>(7);static uintptr_t nextWindow=20;static Proc windowProc;static bool fresh=true;static int wakes=0,cursors=0;
static void battlefield(bool active){memory[0xA8E378]=active;}
static struct {volatile LONG cameraSeq=0;uint32_t cameraMatch=0,cameraTick=0;int cameraTargetX=0,cameraTargetY=0;}mailbox;static auto*shared=&mailbox;
static bool commandFresh(uint32_t){return fresh;}static uint32_t sampleClock(decltype(shared)){return clockValue;}static void wakeGame(){++wakes;}static void MemoryBarrier(){}static void InterlockedIncrement(volatile LONG*p){++*p;}
static HDC CreateCompatibleDC(HDC){return reinterpret_cast<HDC>(1);}static HBITMAP CreateCompatibleBitmap(HDC,int,int){return reinterpret_cast<HBITMAP>(2);}static HGDIOBJ SelectObject(HDC,HGDIOBJ){return reinterpret_cast<HGDIOBJ>(3);}static void DeleteObject(HGDIOBJ){}static void DeleteDC(HDC){}static bool BitBlt(HDC,int,int,int,int,HDC,int,int,int){return true;}
static void ShowWindow(HWND w,int){win[w].visible=false;}static void DestroyWindow(HWND w){win.erase(w);}static void SetCursor(void*){++cursors;}static LONG_PTR SetWindowLongPtrW(HWND w,int,LONG_PTR data){return win[w].data=data;}static LONG_PTR GetWindowLongPtrW(HWND w,int){return win[w].data;}
static LRESULT DefWindowProcW(HWND,UINT,WPARAM,LPARAM){return 0;}static HDC BeginPaint(HWND,PAINTSTRUCT*){return reinterpret_cast<HDC>(1);}static void EndPaint(HWND,PAINTSTRUCT*){}static void GetClientRect(HWND w,RECT*r){*r={0,0,win[w].rect.right-win[w].rect.left,win[w].rect.bottom-win[w].rect.top};}
static int GetModuleHandleW(void*){return 1;}static void RegisterClassW(WNDCLASSW*c){windowProc=c->lpfnWndProc;}
static HWND CreateWindowExW(int,const wchar_t*,const wchar_t*,int,int,int,int,int,void*,void*,int,void*data){HWND w=reinterpret_cast<HWND>(nextWindow++);CREATESTRUCTW c{data};windowProc(w,WM_NCCREATE,0,reinterpret_cast<LPARAM>(&c));return w;}
static void SetLayeredWindowAttributes(HWND,int,int,int){}static void GetWindowRect(HWND w,RECT*r){*r=win[w].rect;}static bool IsWindowVisible(HWND w){return win[w].visible;}
static bool SetWindowPos(HWND w,HWND,int x,int y,int width,int height,int flags){assert(flags&SWP_NOACTIVATE);win[w].rect={x,y,x+width,y+height};win[w].visible=true;++win[w].raises;return true;}
static void InvalidateRect(HWND w,void*,bool){++win[w].invalidations;}
static int synchronousPaints=0;static void UpdateWindow(HWND w){++synchronousPaints;windowProc(w,WM_PAINT,0,0);}
#include "../source/watch_windows.h"
int main(){
 battlefield(true);auto put=[](uint32_t p,auto v){memcpy(memory.data()+p,&v,sizeof v);};
 uint32_t p=0x10000;put(p,0x7F5C70u);put(p+0x10,42u);put(p+0x6C,100);put(p+0x74,uint8_t(1));put(p+0x90,uint8_t(1));put(p+0x9C,40*256);put(p+0xA0,30*256);put(p+0xA4,208);put(0xB0CD48,.144337567);
 for(int i=0;i<3;++i){watchhost::tracks[i].target={p,42};watchhost::views[i].shown=true;watchhost::views[i].frame.focus={p,42};}watchhost::arrange();
 RECT client{0,0,1600,1200};POINT origin{100,200};watchwindows::update(origin,client);assert(win.size()==3&&synchronousPaints==3);for(auto w:watchwindows::windows){assert(win[w].visible&&win[w].raises==1&&win[w].invalidations==1);}
 Graphics clipping;watchwindows::occlude(clipping);assert(excluded.size()==3);for(int i=0;i<3;++i){auto b=watchhost::views[i].box;assert(excluded[i].x==b.x-1&&excluded[i].y==b.y-1&&excluded[i].w==b.w+2&&excluded[i].h==b.h+2);}
 int paints=synchronousPaints;for(int n=0;n<100;++n){for(auto&v:watchhost::views)++v.revision;watchwindows::update(origin,client);}assert(synchronousPaints==paints+300);
 HWND middle=watchwindows::windows[1];auto box=watchhost::views[1].box;assert(win[middle].rect.left==100+box.x*2&&win[middle].rect.top==200+box.y*2);
 assert(watchwindows::proc(middle,WM_MOUSEACTIVATE,0,0)==MA_NOACTIVATE);watchwindows::proc(middle,WM_LBUTTONDOWN,0,0);assert(wakes==1&&shared->cameraSeq==2&&shared->cameraMatch==1&&shared->cameraTargetX==300&&shared->cameraTargetY==1020);
 watchwindows::proc(middle,WM_LBUTTONUP,0,0);assert(wakes==1);watchwindows::proc(middle,WM_RBUTTONDOWN,0,0);assert(watchhost::tracks[1]);
 for(int scenario=0;scenario<4;++scenario){battlefield(scenario!=0);focused=scenario!=1;fresh=scenario!=2;snap.chat=scenario==3;watchwindows::proc(middle,WM_LBUTTONDOWN,0,0);assert(wakes==1);}battlefield(true);focused=fresh=true;snap.chat=0;
 clockValue+=999;watchwindows::update(origin,client);assert(win[middle].raises==1);++clockValue;watchwindows::update(origin,client);assert(win[middle].raises==2&&win[middle].invalidations==102);
 battlefield(false);watchwindows::update(origin,client);for(auto w:watchwindows::windows)assert(!win[w].visible);clockValue+=2000;watchwindows::update(origin,client);assert(win[middle].raises==2);battlefield(true);watchwindows::update(origin,client);assert(win[middle].visible&&win[middle].raises==3);
 watchwindows::proc(middle,WM_RBUTTONDOWN,MK_SHIFT,0);assert(!watchhost::tracks[1]&&!watchhost::views[1].shown&&!win[middle].visible&&watchhost::tracks[0]&&watchhost::tracks[2]);clockValue+=1000;watchwindows::update(origin,client);assert(!win[middle].visible);
 watchwindows::proc(watchwindows::windows[0],WM_SETCURSOR,0,0);assert(cursors==1);watchwindows::destroy();assert(win.empty());
 std::cout<<"PASS production window handlers: one-click native-camera mailbox, independent Shift-right-close, no activation, physical scaling, one-second reassertion, menu/focus/chat gating and destroy\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
 cpp=Path(tmp)/'watch.cpp';exe=Path(tmp)/'watch';cpp.write_text(prefix+shim)
 subprocess.run(['g++','-std=c++17','-O1','-I'+str(root/'assist/tests'),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
