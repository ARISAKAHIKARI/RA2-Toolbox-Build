#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <vector>
#include <cstdio>
#include "../source/radar_panel.h"
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL line %d: %s error=%lu\n",__LINE__,#x,GetLastError());ExitProcess(2);}}while(0)
struct Image {int width=16,height=8;std::vector<uint32_t> pixels=std::vector<uint32_t>(128,0xff426839u);};
int main(){
 HDC dc=CreateCompatibleDC(nullptr);CHECK(dc);BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bitmap.bmiHeader.biWidth=80;bitmap.bmiHeader.biHeight=-60;bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;void*bits=nullptr;
 HBITMAP image=CreateDIBSection(dc,&bitmap,DIB_RGB_COLORS,&bits,nullptr,0);CHECK(image&&bits);auto old=SelectObject(dc,image);CHECK(old&&old!=HGDI_ERROR);
 radarpanel::Buffer buffer;Image source;radargate::Visual visual;visual.width=34;visual.height=18;visual.marks={{3,3,6,6,0xe02010}};visual.view={10,10,20,14,0};
 CHECK(buffer.stage(dc,source,visual));CHECK(!buffer.ready(34,18));CHECK(!buffer.present(dc,34,18));CHECK(buffer.promote());CHECK(buffer.present(dc,34,18));CHECK(GdiFlush());
 CHECK(GetPixel(dc,8,8)==RGB(0x42,0x68,0x39));CHECK(GetPixel(dc,3,3)==RGB(0xe0,0x20,0x10));CHECK(GetPixel(dc,0,0)==RGB(157,161,115));CHECK(GetPixel(dc,10,10)==RGB(230,234,205));
 auto rejected=source;rejected.pixels.pop_back();CHECK(!buffer.stage(dc,rejected,visual));CHECK(!buffer.promote());CHECK(!buffer.stage(nullptr,source,visual));CHECK(buffer.ready(34,18));CHECK(buffer.present(dc,34,18));CHECK(GetPixel(dc,3,3)==RGB(0xe0,0x20,0x10));
 source.pixels.assign(source.pixels.size(),0xff204080u);visual.width=18;visual.height=10;visual.marks.clear();visual.view={};CHECK(buffer.stage(dc,source,visual));CHECK(buffer.ready(34,18)&&!buffer.ready(18,10));CHECK(buffer.present(dc,34,18));CHECK(GetPixel(dc,8,8)==RGB(0x42,0x68,0x39));
 CHECK(buffer.promote());CHECK(buffer.ready(18,10));CHECK(!buffer.present(nullptr,18,10)&&!buffer.painted);CHECK(buffer.present(dc,18,10)&&buffer.painted);CHECK(GetPixel(dc,8,7)==RGB(0x20,0x40,0x80));CHECK(!buffer.present(dc,34,18));
 // Warm both retained buffers. No per-marker Create/DeleteObject churn.
 CHECK(buffer.stage(dc,source,visual));CHECK(buffer.promote());DWORD handles=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);CHECK(handles);
 visual.marks.assign(2000,{3,3,6,6,0x1070a0});for(int i=0;i<50;++i){CHECK(buffer.stage(dc,source,visual));CHECK(buffer.promote());CHECK(buffer.present(dc,18,10));}CHECK(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==handles);
 buffer.clear();CHECK(!buffer.ready(18,10)&&!buffer.present(dc,18,10));SelectObject(dc,old);DeleteObject(image);DeleteDC(dc);
 std::puts("PASS real GDI minimap staging: complete front survives failed/new-size preparation, explicit promotion, exact markers/border/view colors, failed-present retry, native/HALFTONE copies and zero warm GDI handle churn for100000 marker draws");
}
