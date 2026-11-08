"""Execute production async reservation capture/promotion and WM_PAINT with GDI doubles.
The worker is controlled; no game or DWM claim. Pending/failure frames must never
clear the visible surface or expose hit regions for a different tab/scroll/queue.
"""
from pathlib import Path
import os,subprocess,tempfile
from tactical_frame_sampling import function
root=Path(__file__).resolve().parents[2]
host=Path(os.environ.get('CONSTRUCTION_HOST',root/'assist/source/construction_host.h')).read_text()
position=host[host.index('static void resizePanel()'):host.index('static bool usable();')]
types=host[host.index('struct PaintRequest {'):host.index('struct PanelPainter {')]
modern='static std::shared_ptr<const Painted> paintedFrame;' in host
helpers=host[host.index('static std::shared_ptr<const Painted> paintedFrame;' if modern else 'static LRESULT CALLBACK proc('):host.index('static void service()')]
scope=function((root/'external_reader/source/process_reader.h').read_text(), 'class WorldFrameScope')+';'
prefix=r'''

#include "construction_policy.h"
#include "fixtures/tactical_frame/sealed_host_reader.h"
#include <cassert>
#include <string>
#include <iostream>
#include <cstdint>
#include <memory>
#include <map>
#include <cstring>
using namespace assist;
using DWORD=uint32_t;using UINT=unsigned;using WPARAM=uintptr_t;using LPARAM=intptr_t;using LRESULT=intptr_t;
#define CALLBACK
struct RECT{int left=0,top=0,right=0,bottom=0;};struct POINT{int x=0,y=0;};
struct Window{RECT bounds{};bool visible=false;};using HWND=Window*;
struct Object{int kind=0,w=0,h=0;uint32_t color=0;std::vector<uint32_t> pixels;};
using COLORREF=uint32_t;using HPEN=Object*;using HGDIOBJ=Object*;using HBITMAP=Object*;using HBRUSH=Object*;using HFONT=Object*;
static Object stockBitmap{1},stockFont{2},testFont{2},stockBrush{3},stockPen{4};
struct DC{HBITMAP bitmap=&stockBitmap;HFONT font=&stockFont;HBRUSH brush=&stockBrush;HPEN pen=&stockPen;POINT cursor{};uint32_t text=0;};using HDC=DC*;
static DC screen;struct PAINTSTRUCT{};
static int fail=0,dcLive=0,bitmapLive=0,allocations=0,blits=0,positions=0,paintStarts=0,paintEnds=0,lastFlags=0;
static bool originReady=true;static POINT origin{10,20};
static HGDIOBJ HGDI_ERROR=reinterpret_cast<HGDIOBJ>(intptr_t(-1));
constexpr int WM_MOUSEACTIVATE=1,WM_ERASEBKGND=2,WM_MOUSEWHEEL=3,WM_LBUTTONUP=4,WM_RBUTTONUP=5,WM_PAINT=6,WM_MOUSEMOVE=7,WM_MOUSELEAVE=8;
constexpr int PS_SOLID=0,TME_LEAVE=2;struct TRACKMOUSEEVENT{size_t cbSize;unsigned flags;HWND hwnd;unsigned hover;};static void TrackMouseEvent(TRACKMOUSEEVENT*){}
constexpr int MA_NOACTIVATE=3,FALSE=0,SW_HIDE=0,MK_SHIFT=4,TRANSPARENT=1,DT_CENTER=1,DT_VCENTER=2,DT_SINGLELINE=4,DT_WORDBREAK=8,DT_END_ELLIPSIS=16;
struct LOGFONTW{int fields[16]{};};struct BITMAPINFOHEADER{unsigned biSize=0;int biWidth=0,biHeight=0;unsigned short biPlanes=0,biBitCount=0;};struct BITMAPINFO{BITMAPINFOHEADER bmiHeader;};constexpr int DIB_RGB_COLORS=0;
constexpr int SRCCOPY=1,SWP_NOACTIVATE=1,SWP_SHOWWINDOW=2,SWP_NOOWNERZORDER=4,SWP_NOCOPYBITS=8,SWP_NOZORDER=16;
static HWND HWND_TOPMOST=nullptr;
static uint32_t RGB(unsigned r,unsigned g,unsigned b){return r|(g<<8)|(b<<16);}
static unsigned HIWORD(uintptr_t n){return (n>>16)&65535;}static unsigned LOWORD(uintptr_t n){return n&65535;}
static HDC CreateCompatibleDC(HDC){if(fail==1)return nullptr;++dcLive;return new DC;}
static HBITMAP CreateCompatibleBitmap(HDC,int w,int h){if(fail==2)return nullptr;++bitmapLive;++allocations;auto p=new Object{1,w,h};p->pixels.resize(w*h,0xDEADBEEF);return p;}
static HGDIOBJ SelectObject(HDC dc,HGDIOBJ object){
 if(fail==3&&object->kind==1&&object!=&stockBitmap)return HGDI_ERROR;
 if(object->kind==1){auto old=dc->bitmap;dc->bitmap=object;return old;}
 if(object->kind==3){auto old=dc->brush;dc->brush=object;return old;}if(object->kind==4){auto old=dc->pen;dc->pen=object;return old;}
 assert(object->kind==2);auto old=dc->font;dc->font=object;return old;
}
static bool DeleteObject(HGDIOBJ p){assert(p!=&stockBitmap&&p!=&stockFont&&p!=&testFont);if(p->kind==1)--bitmapLive;delete p;return true;}
static bool DeleteDC(HDC dc){assert(dc->bitmap==&stockBitmap);--dcLive;delete dc;return true;}
static HBRUSH CreateSolidBrush(uint32_t c){auto p=new Object{3};p->color=c;return p;}
static void fill(HDC dc,const RECT&r,uint32_t c){
 assert(dc!=&screen && "partial background/card/text reached the visible window");
 auto p=dc->bitmap;for(int y=std::max(0,r.top);y<std::min(p->h,r.bottom);++y)for(int x=std::max(0,r.left);x<std::min(p->w,r.right);++x)p->pixels[y*p->w+x]=c;
}
static void FillRect(HDC dc,const RECT*r,HBRUSH b){fill(dc,*r,b->color);}
static HPEN CreatePen(int,int,uint32_t c){auto p=new Object{4};p->color=c;return p;}
static void RoundRect(HDC dc,int x,int y,int r,int b,int,int){fill(dc,{x,y,r,b},dc->brush->color);}
static void MoveToEx(HDC dc,int x,int y,void*){dc->cursor={x,y};}
static void LineTo(HDC dc,int x,int y){fill(dc,{std::min(x,dc->cursor.x),std::min(y,dc->cursor.y),std::max(x,dc->cursor.x)+1,std::max(y,dc->cursor.y)+1},dc->pen->color);dc->cursor={x,y};}
static void SetBkMode(HDC,int){}static void SetTextColor(HDC dc,uint32_t c){dc->text=c;}
static void DrawTextW(HDC dc,const wchar_t*t,int,RECT*r,int){if(*t)fill(dc,{r->left+1,r->top+1,r->right-1,std::min(r->bottom,r->top+3)},dc->text);}
static bool BitBlt(HDC to,int,int,int w,int h,HDC from,int,int,int){
 if(fail==6)return false;assert(to==&screen&&from!=&screen&&from->bitmap->w==w&&from->bitmap->h==h);
 assert(std::find(from->bitmap->pixels.begin(),from->bitmap->pixels.end(),0xDEADBEEF)==from->bitmap->pixels.end());
 to->bitmap->w=w;to->bitmap->h=h;to->bitmap->pixels=from->bitmap->pixels;++blits;return true;
}
static HDC BeginPaint(HWND,PAINTSTRUCT*){++paintStarts;return &screen;}static void EndPaint(HWND,PAINTSTRUCT*){++paintEnds;}
static bool GetClientRect(HWND w,RECT*r){if(fail==9)return false;*r={0,0,w->bounds.right-w->bounds.left,w->bounds.bottom-w->bounds.top};return true;}
static bool ClientToScreen(HWND,POINT*p){*p=origin;return originReady;}
static void ShowWindow(HWND w,int){w->visible=false;}static bool IsWindowVisible(HWND w){return w->visible;}static bool GetWindowRect(HWND w,RECT*r){*r=w->bounds;return true;}
static void SetWindowPos(HWND w,HWND,int x,int y,int width,int height,int flags){++positions;lastFlags=flags;w->bounds={x,y,x+width,y+height};if(flags&SWP_SHOWWINDOW)w->visible=true;}
namespace overlaystack {enum class Layer {Construction};static void place(HWND w,Layer,int x,int y,int width,int height,int flags){SetWindowPos(w,HWND_TOPMOST,x,y,width,height,flags);}}
static unsigned invalidations=0;static void InvalidateRect(HWND,void*,int){++invalidations;}static LRESULT DefWindowProcW(HWND,UINT,WPARAM,LPARAM){return 0;}
#include "construction_buffer.h"
static Window gameObject,panelObject;static HWND game=&gameObject,window=&panelObject;
static construction::Layout layout;static construction::PanelBuffer panelBuffer,panelSpare;static construction::Layout paintedLayout;static bool paintReady=false;
static bool geometryReady=true;static int activeTab=0,selectedTab=-1,scroll=0,hoverChoice=-1,hoverQueue=-1;static HFONT panelFont=&testFont,font=&testFont;
static construction::Book books[4];struct Choice{int slot,index,tab;uint32_t type;bool future;};static std::vector<Choice> choices;
namespace assist{struct Type{char id[24]{};uint16_t name[40]{};int kind=7,index=0,naval=0,cost=0;};}
static struct{int typeCount=8;assist::Type types[8];}snap;
static int encode(int kind,int index){return kind==7?index:(kind<<16)|index;}static void refreshChoices(){}
static int lastReservation=-1;static unsigned reservationCalls=0;
static bool usable(){return true;}static void removeTop(bool){}static void removeItem(int,int,bool){}static void reserve(int index){lastReservation=index;++reservationCalls;}

static uint32_t pid=1,house=0x500000,match=7,now=1000;static bool eligible=true;
static DWORD GetTickCount(){return now;}static uint32_t commandMatch(){return match;}
static bool displayReady(){return eligible;}static bool constructionDisplayFresh(){return eligible;}
static int typeSlot(int index){return index>=0&&index<snap.typeCount?index:-1;}
static uint32_t typePointer(int index){return 0x600000+uint32_t(index)*0x1000;}
static void GetObjectW(HFONT,int bytes,void*out){std::memset(out,0,size_t(bytes));}
static HDC GetDC(HWND){return fail==7?nullptr:&screen;}static void ReleaseDC(HWND,HDC){}
static bool GdiFlush(){return fail!=5;}
static UINT SetDIBitsToDevice(HDC to,int,int,int w,int h,int,int,UINT,UINT,const void*data,const BITMAPINFO*,int){
 assert(to!=&screen);if(fail==4)return 0;assert(to->bitmap->w==w&&to->bitmap->h==h);std::memcpy(to->bitmap->pixels.data(),data,size_t(w)*h*4);return UINT(h);
}
'''
prefix+=r'''
struct Reader:fixtureframe::SealedHostReader<Reader>{bool readLive(uint32_t,void*,size_t){assert(false);return false;}}externalReader;
static void publishFrame(){auto next=std::make_shared<worldframe::Snapshot>();worldframe::Key key{};key.pid=pid;key.match=match;key.frame=int(now);next->reset(key);assert(next->seal());externalReader.available=std::move(next);}
'''
prefix+='namespace external {using ProcessReader=::Reader;'+scope+'}\n'
worker=r'''
struct FakeWorker{unsigned submitted=0,invalidated=0;std::shared_ptr<const Painted> result;std::shared_ptr<PaintRequest> pending;
 void submit(PaintRequest r){++submitted;pending=std::make_shared<PaintRequest>(std::move(r));}
 std::shared_ptr<const Painted> get(){return result;}
 void invalidate(){++invalidated;result.reset();pending.reset();}
}paintWorker;
'''
suffix=r'''
static void complete(uint32_t color=0xff123456,bool incomplete=false){assert(paintWorker.pending);auto p=std::make_shared<Painted>();p->request=*paintWorker.pending;p->pixels.assign(size_t(p->request.width)*p->request.height,color);p->incomplete=incomplete;paintWorker.result=p;}
static void paint(){proc(window,WM_PAINT,0,0);assert(paintStarts==paintEnds);}
int main(){
 Object visible{1};screen.bitmap=&visible;
 for(int n=0;n<8;++n){snap.types[n].index=n;snap.types[n].id[0]=char('A'+n);}
 assert(layout.set(2560,1440,3840,2160));layout.content(5,8);books[0].items={0,1,2,3,4};
 for(int i=0;i<8;++i)choices.push_back({i,i,0,0x600000u+unsigned(i)*0x1000,i>4});
 requestPaint();assert(paintWorker.submitted==1);resizePanel();paint();assert(!panelObject.visible&&positions==0&&blits==0);
 complete();pumpPaint();assert(panelObject.visible&&positions==1&&paintReady&&panelInputReady());paint();assert(blits==1);
 const auto first=visible.pixels;unsigned submissions=paintWorker.submitted;
 // Background service/expose events keep one complete front frame, no new
 // raster, bitmap allocation or layer reordering while the content is stable.
 for(int i=0;i<1000;++i){now+=100;requestPaint();pumpPaint();resizePanel();paint();}
 assert(paintWorker.submitted==submissions&&positions==1&&allocations==1&&visible.pixels==first);
 // A changed tab/scroll/queue cannot erase the old frame; input closes until
 // the new complete pixels and geometry have both been promoted.
 activeTab=1;books[1].items={2};layout.content(1,8);requestPaint();assert(!panelInputReady());paint();assert(visible.pixels==first&&positions==1);
 complete(0xff654321);pumpPaint();assert(panelInputReady()&&positions==2);paint();assert(visible.pixels!=first);
 fail=9;assert(!panelInputReady());fail=0;assert(panelInputReady());
 auto before=visible.pixels;int beforePositions=positions;scroll=2;requestPaint();paint();assert(visible.pixels==before&&!panelInputReady());
 // GDI target/DC/bitmap/selection/upload/flush failures are transactional.
 complete(0xffaabbcc);for(int mode:{7,4,5}){fail=mode;pumpPaint();paint();assert(visible.pixels==before&&positions==beforePositions&&!panelInputReady());}fail=0;pumpPaint();paint();assert(panelInputReady()&&visible.pixels[0]==0xffaabbcc);
 // Size failure retains the old buffer and on-screen rectangle.
 layout.content(5,8);books[1].items={1,2,3,4,5};requestPaint();complete(0xff778899);before=visible.pixels;beforePositions=positions;
 for(int mode:{1,2,3}){fail=mode;pumpPaint();paint();assert(visible.pixels==before&&positions==beforePositions&&!panelInputReady());}fail=0;pumpPaint();paint();assert(panelInputReady());
 // Hover-only visual changes do not close an otherwise matching action map.
 hoverChoice=3;requestPaint();assert(panelInputReady());complete(0xff111222);pumpPaint();paint();assert(visible.pixels[0]==0xff111222);
 // Missing icons retry at a bounded interval; success becomes fully idle.
 hoverChoice=4;requestPaint();complete(0xff111222,true);pumpPaint();unsigned count=paintWorker.submitted;now+=999;requestPaint();assert(paintWorker.submitted==count);++now;requestPaint();assert(paintWorker.submitted==count+1);complete();pumpPaint();now+=2000;requestPaint();assert(paintWorker.submitted==count+1);
 // A lost/failing visible BitBlt retries presentation, not asset raster.
 fail=6;paint();unsigned inv=invalidations;now+=249;pumpPaint();assert(invalidations==inv);++now;pumpPaint();assert(invalidations==inv+1);fail=0;paint();
 // Explicit hide/invalidation clears both visual and pending identities.
 resetPaint();assert(!paintReady&&!panelInputReady()&&paintWorker.invalidated&&dcLive==0&&bitmapLive==0);paint();
 requestPaint();complete();++match;pumpPaint();assert(!paintReady);--match;pumpPaint();assert(paintReady);
 ++match;requestPaint();assert(!paintReady&&!panelObject.visible);complete();pumpPaint();assert(paintReady&&panelInputReady());resetPaint();

 // A stationary screen point keeps selecting the first choice after every
 // complete queue promotion. Pending pixels never admit the new input map.
 activeTab=selectedTab=scroll=0;POINT fixed{};bool firstQueue=true;
 for(int count:{0,1,2,3,4,5,32,128,4,1,0}){
  books[0].items.assign(count,0);layout.content(count,int(choices.size()));requestPaint();
  assert(!panelInputReady());unsigned calls=reservationCalls;
  proc(window,WM_LBUTTONUP,0,0);assert(reservationCalls==calls);
  complete();pumpPaint();assert(panelInputReady());
  POINT center{window->bounds.left+layout.x(36),window->bounds.top+layout.pointY(layout.headerHeight()+33)};
  if(firstQueue){fixed=center;firstQueue=false;}else assert(center.x==fixed.x&&center.y==fixed.y);
  const int x=fixed.x-window->bounds.left,y=fixed.y-window->bounds.top;
  proc(window,WM_LBUTTONUP,0,LPARAM(uint32_t(x)|(uint32_t(y)<<16)));
  assert(reservationCalls==calls+1&&lastReservation==0);
 }
 resetPaint();
 std::cout<<"PASS stationary screen click across actual queue frame promotion 0..128..0; pending frames reject input\n";
 std::cout<<"PASS production reservation async lifecycle:1000 idle updates no reraster/reposition; firstshowaftercomplete; tab/scroll/queue/geometry promoted with pixels; no background-only frames; input map guarded\n";
 std::cout<<"PASS transactional DC/bitmap/selection/upload/flush failure, bounded missing-icon and BitBlt retries, hover-only interaction, world invalidation and cleanup\n";
}
'''
if not modern:
    # Run the original shipping WM_PAINT against the same visible-write guard.
    # A pending first async result must not publish a solid background frame.
    suffix=r'''
int main(){Object visible{1};screen.bitmap=&visible;layout.set(2560,1440,3840,2160);layout.content(0,0);resizePanel();proc(window,WM_PAINT,0,0);}
'''
suffix=suffix.replace('requestPaint();','publishFrame();requestPaint();')
with tempfile.TemporaryDirectory() as tmp:
    cpp=Path(tmp)/'paint.cpp';exe=Path(tmp)/'paint';cpp.write_text(prefix+position+types+worker+helpers+suffix)
    subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'assist/source'),'-I'+str(root/'assist/tests'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
