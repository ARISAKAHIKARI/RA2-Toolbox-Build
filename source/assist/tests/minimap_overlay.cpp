#include <algorithm>
#include <cstdint>
#include <cassert>
#include <iostream>
#include "../source/protocol.h"
using HWND=void*;using HDC=void*;using HBITMAP=void*;using HBRUSH=void*;using LRESULT=intptr_t;using WPARAM=uintptr_t;using LPARAM=intptr_t;using UINT=unsigned;using DWORD=uint32_t;using BOOL=int;
#define CALLBACK
#define HWND_TOPMOST ((HWND)-1)
constexpr int HTTRANSPARENT=-1,MA_NOACTIVATE=3,WM_NCHITTEST=1,WM_MOUSEACTIVATE=2,WM_ERASEBKGND=3,WM_PAINT=4,SW_HIDE=0,SWP_NOACTIVATE=1,SWP_SHOWWINDOW=2,FALSE=0,WS_POPUP=8,WS_EX_TOOLWINDOW=16,WS_EX_NOACTIVATE=32,WS_EX_TOPMOST=64,WS_EX_TRANSPARENT=128,WS_EX_LAYERED=256,LWA_ALPHA=2,COLORONCOLOR=3,SRCCOPY=4,CAPTUREBLT=8;
struct POINT {int x,y;};struct RECT{int left=0,top=0,right=0,bottom=0;};struct PAINTSTRUCT{};struct WNDCLASSW {LRESULT(*lpfnWndProc)(HWND,UINT,WPARAM,LPARAM);int hInstance;const wchar_t*lpszClassName;};
static bool enabled=true,minimapEnabled=true,foreground=true,copyOK=true;static int minimapScale=200,fps=500;static assist::Snapshot snap;static DWORD now=1000;
static int copies=0,hides=0,dcCount=0,bitmapCount=0,dcRemoved=0,bitmapRemoved=0,style=0;static RECT source,destination;
static bool foregroundGame(){return foreground;}static void ShowWindow(HWND,int){++hides;}static HBITMAP SelectObject(HDC,HBITMAP){return (HBITMAP)5;}static void DeleteObject(HBITMAP){++bitmapRemoved;}static void DeleteDC(HDC){++dcRemoved;}
static void DestroyWindow(HWND){}static LRESULT DefWindowProcW(HWND,UINT,WPARAM,LPARAM){return 0;}
static HDC BeginPaint(HWND,PAINTSTRUCT*){return (HDC)1;}static void EndPaint(HWND,PAINTSTRUCT*){}static bool GetClientRect(HWND,RECT*r){r->right=640;r->bottom=560;return true;}
static void SetStretchBltMode(HDC,int){}static void StretchBlt(HDC,int,int,int,int,HDC,int,int,int,int,int){}static HBRUSH CreateSolidBrush(int){return (HBRUSH)1;}static int RGB(int,int,int){return 1;}static void FrameRect(HDC,RECT*,HBRUSH){}
static int GetModuleHandleW(void*){return 1;}static void RegisterClassW(WNDCLASSW*){}
static HWND CreateWindowExW(int s,const wchar_t*,const wchar_t*,int,int,int,int,int,void*,void*,int,void*){style=s;return (HWND)1;}static void SetLayeredWindowAttributes(HWND,int,int,int){}
static DWORD GetTickCount(){return now;}static HDC GetDC(void*){return (HDC)2;}static HDC CreateCompatibleDC(HDC){++dcCount;return (HDC)3;}static HBITMAP CreateCompatibleBitmap(HDC,int,int){++bitmapCount;return (HBITMAP)4;}static void ReleaseDC(void*,HDC){}
static BOOL BitBlt(HDC,int,int,int w,int h,HDC,int x,int y,int){source={x,y,x+w,y+h};++copies;return copyOK;}
static void InvalidateRect(HWND,void*,bool){}static void SetWindowPos(HWND,HWND,int x,int y,int w,int h,int){destination={x,y,x+w,y+h};}
// Production header uses nullable native handles. Match that ABI in the test.

#include "../source/minimap_overlay.h"
int main(){
 snap.width=1920;snap.height=1080;snap.renderStatus=1;RECT client{0,0,3840,2160};POINT origin{100,200};
 minimap::update(origin,client);assert(copies==1&&source.left==3620&&source.top==200&&source.right==3940&&source.bottom==480);
 assert(destination.right<source.left&&destination.left>=origin.x&&destination.bottom<=origin.y+client.bottom);
 assert((style&(WS_EX_TRANSPARENT|WS_EX_LAYERED|WS_EX_NOACTIVATE))==(WS_EX_TRANSPARENT|WS_EX_LAYERED|WS_EX_NOACTIVATE));
 assert(minimap::proc(nullptr,WM_NCHITTEST,0,0)==HTTRANSPARENT&&minimap::proc(nullptr,WM_MOUSEACTIVATE,0,0)==MA_NOACTIVATE);
 minimap::update(origin,client);assert(copies==1);now+=16;minimap::update(origin,client);assert(copies==2);
 for(int gw:{800,1920,2560,3840})for(int gh:{600,1080,1440,2160})for(int cw:{640,1920,3840})for(int ch:{480,1080,2160})for(int scale:{100,200,400}){
  snap.width=gw;snap.height=gh;minimapScale=scale;client={0,0,cw,ch};now+=20;minimap::update(origin,client);
  assert(source.left==origin.x+cw-std::max(1,int(int64_t(160)*cw/gw))&&source.right==origin.x+cw);
  assert(destination.right<source.left&&destination.left>=origin.x&&destination.bottom<=origin.y+ch);
 }
 int count=copies;foreground=false;minimap::update(origin,client);assert(copies==count);foreground=true;snap.renderStatus=0;minimap::update(origin,client);assert(copies==count);
 snap.renderStatus=1;copyOK=false;now+=20;int hidden=hides;minimap::update(origin,client);assert(hides>hidden);
 minimap::destroy();assert(dcCount==dcRemoved&&bitmapCount==bitmapRemoved);
 std::cout<<"PASS: 432 game/client/zoom combinations; unobstructed source, 60 Hz cap, transparent input, menu/focus/capture failure and resource lifetime\n";
}
