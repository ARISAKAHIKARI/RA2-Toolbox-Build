#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

static unsigned checks=0,dcCreates=0,bitmapCreates=0;
static int allocationFailure=0;
static void check(bool value,int line,const char*expression){
 ++checks;if(value)return;
 std::fprintf(stderr,"FAIL construction buffer line %d: %s error=%lu\n",line,expression,GetLastError());
 std::fflush(stderr);ExitProcess(2);
}
#define CHECK(value) check(bool(value),__LINE__,#value)

// Exercise the production allocation-failure paths with real GDI resources
// around the one failed call; do not induce process-wide GDI exhaustion.
static HDC allocateDC(HDC target){if(allocationFailure==1)return nullptr;auto dc=CreateCompatibleDC(target);if(dc)++dcCreates;return dc;}
static HBITMAP allocateBitmap(HDC target,int width,int height){if(allocationFailure==2)return nullptr;auto bitmap=CreateCompatibleBitmap(target,width,height);if(bitmap)++bitmapCreates;return bitmap;}
static HGDIOBJ selectBitmap(HDC dc,HGDIOBJ object){if(allocationFailure==3&&GetObjectType(object)==OBJ_BITMAP)return HGDI_ERROR;return SelectObject(dc,object);}
#define CreateCompatibleDC allocateDC
#define CreateCompatibleBitmap allocateBitmap
#define SelectObject selectBitmap
#include "../source/construction_buffer.h"
#undef CreateCompatibleDC
#undef CreateCompatibleBitmap
#undef SelectObject

static constexpr int TargetSize=1024;
static constexpr uint32_t Sentinel=0x00a1b2c3;
static std::vector<uint32_t> frame(int width,int height,unsigned serial){
 std::vector<uint32_t> pixels(size_t(width)*height);
 for(int y=0;y<height;++y)for(int x=0;x<width;++x)pixels[size_t(y)*width+x]=
  (((x*17+serial*11)&255)<<16)|(((y*23+serial*7)&255)<<8)|((x+y+serial*29)&255);
 return pixels;
}
static void draw(HDC dc,int width,int height,const std::vector<uint32_t>&pixels){
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;
 info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 CHECK(SetDIBitsToDevice(dc,0,0,width,height,0,0,0,height,pixels.data(),&info,DIB_RGB_COLORS)==UINT(height));
 CHECK(GdiFlush());
}
static void unchanged(const uint32_t*pixels){CHECK(std::all_of(pixels,pixels+size_t(TargetSize)*TargetSize,[](uint32_t p){return (p&0xffffff)==Sentinel;}));}
static void equal(const uint32_t*actual,int width,int height,const std::vector<uint32_t>&expected){
 for(int y=0;y<TargetSize;++y)for(int x=0;x<TargetSize;++x){const uint32_t value=x<width&&y<height?expected[size_t(y)*width+x]:Sentinel;
  CHECK((actual[size_t(y)*TargetSize+x]&0xffffff)==value);
 }
}
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 HANDLE done=CreateEventW(nullptr,TRUE,FALSE,nullptr);CHECK(done);
 // No HWND is owned by this fixture, so the independent bounded watchdog
 // needs no UI wait/message pump and cannot block another window owner.
 std::thread watchdog([done]{if(WaitForSingleObject(done,20000)!=WAIT_OBJECT_0)TerminateProcess(GetCurrentProcess(),124);});
 HDC screen=GetDC(nullptr);CHECK(screen);HDC target=CreateCompatibleDC(screen);CHECK(target);
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=TargetSize;info.bmiHeader.biHeight=-TargetSize;
 info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 void*bits=nullptr;HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&bits,nullptr,0);CHECK(bitmap&&bits);
 auto previous=SelectObject(target,bitmap);CHECK(previous&&previous!=HGDI_ERROR);CHECK(ReleaseDC(nullptr,screen));
 auto pixels=static_cast<uint32_t*>(bits);const DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);CHECK(before>0);
 assist::construction::PanelBuffer buffer;
 CHECK(!buffer.present(target));CHECK(!buffer.acquire(target,0,10));CHECK(!buffer.acquire(target,10,0));
 HDC source=buffer.acquire(target,256,144);CHECK(source);HGDIOBJ sourceBitmap=GetCurrentObject(source,OBJ_BITMAP);CHECK(sourceBitmap);
 const unsigned dcOnce=dcCreates,bitmapOnce=bitmapCreates;const DWORD resident=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);CHECK(resident==before+2);
 for(int i=0;i<1000;++i){CHECK(buffer.acquire(target,256,144)==source);CHECK(GetCurrentObject(source,OBJ_BITMAP)==sourceBitmap);}
 CHECK(dcCreates==dcOnce&&bitmapCreates==bitmapOnce&&GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==resident);
 auto original=frame(256,144,1);std::fill_n(pixels,size_t(TargetSize)*TargetSize,Sentinel);draw(source,256,144,original);unchanged(pixels);
 CHECK(buffer.present(target));CHECK(GdiFlush());equal(pixels,256,144,original);
 // The UI may fully upload into a spare backing store and only then swap
 // it into the front. The swap itself must not write the visible target.
 assist::construction::PanelBuffer spare;HDC next=spare.acquire(target,256,144);CHECK(next&&next!=source);
 const auto replacement=frame(256,144,2);draw(next,256,144,replacement);equal(pixels,256,144,original);
 const unsigned warmDC=dcCreates,warmBitmap=bitmapCreates;const DWORD twoBuffers=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);CHECK(twoBuffers==resident+2);
 buffer.swap(spare);equal(pixels,256,144,original);CHECK(buffer.acquire(target,256,144)==next);
 std::fill_n(pixels,size_t(TargetSize)*TargetSize,Sentinel);CHECK(buffer.present(target));CHECK(GdiFlush());equal(pixels,256,144,replacement);
 CHECK(spare.acquire(target,256,144)==source);buffer.swap(spare);
 for(int i=0;i<1000;++i){buffer.swap(spare);CHECK(buffer.acquire(target,256,144)==(i%2?source:next));}
 CHECK(dcCreates==warmDC&&bitmapCreates==warmBitmap&&GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==twoBuffers);
 spare.clear();CHECK(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==resident);
 // An invalid proposal or failed replacement leaves the last complete
 // buffer usable, and cannot expose a partially cleared visible target.
 for(int mode=0;mode<=3;++mode){
  allocationFailure=mode;CHECK(!buffer.acquire(target,mode?300:0,160));allocationFailure=0;
  CHECK(buffer.acquire(target,256,144)==source&&GetCurrentObject(source,OBJ_BITMAP)==sourceBitmap);
  CHECK(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==resident);
  std::fill_n(pixels,size_t(TargetSize)*TargetSize,Sentinel);CHECK(buffer.present(target));CHECK(GdiFlush());equal(pixels,256,144,original);
 }
 unsigned cases=0;
 for(int width:{72,144,216,360})for(int height:{56,132,244,500})for(int extra:{0,1}){
  const int w=width+extra,h=height+extra;source=buffer.acquire(target,w,h);CHECK(source);
  const auto expected=frame(w,h,++cases);std::fill_n(pixels,size_t(TargetSize)*TargetSize,Sentinel);draw(source,w,h,expected);unchanged(pixels);
  CHECK(buffer.present(target));CHECK(GdiFlush());equal(pixels,w,h,expected);
  CHECK(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==resident);
 }
 buffer.clear();CHECK(!buffer.present(target));CHECK(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==before);buffer.clear();
 SelectObject(target,previous);CHECK(DeleteObject(bitmap));CHECK(DeleteDC(target));
 SetEvent(done);watchdog.join();CloseHandle(done);
 std::printf("PASS actual Win32 GDI construction buffer: 1000 stable acquisitions/swaps, %u full-color resize frames, transactional front/spare, failed DC/bitmap/selection preserve old frame, bounded resources and idempotent clear (%u checks); controlled GDI only, not DWM/game proof\n",cases,checks);
}
