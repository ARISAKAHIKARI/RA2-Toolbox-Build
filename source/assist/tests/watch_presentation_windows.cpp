#ifdef NDEBUG
#undef NDEBUG
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../source/watch_panel.h"
#include <cassert>
#include <cstdio>
#include <algorithm>
int main(){
 assert(watchpresentation::queueDelay(100,99)==1&&watchpresentation::queueDelay(99,100)==0);
 assert(watchpresentation::queueDelay(7,0xfffffffeu)==9);
 Gdiplus::GdiplusStartupInput startup;ULONG_PTR token=0;assert(Gdiplus::GdiplusStartup(&token,&startup,nullptr)==Gdiplus::Ok);
 {
  HDC screen=GetDC(nullptr),target=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,640,512);auto old=SelectObject(target,bitmap);ReleaseDC(nullptr,screen);assert(target&&bitmap&&old);
  auto image=std::make_shared<std::vector<uint32_t>>(64*64);
  for(int y=0;y<64;++y)for(int x=0;x<64;++x)(*image)[y*64+x]=y<32?(x<32?0xffff0000u:0xff00ff00u):(x<32?0xff0000ffu:0xffffffffu);
  watchpresentation::Visual v;v.image=image;v.sourceWidth=v.sourceHeight=64;v.width=v.height=64;
  watchpanel::Buffer buffer;assert(buffer.needs(v)&&buffer.stage(target,image,v)&&!buffer.needs(v)&&buffer.present(target,64,64));
  assert(GetPixel(target,16,16)==RGB(255,0,0)&&GetPixel(target,48,16)==RGB(0,255,0)&&GetPixel(target,16,48)==RGB(0,0,255)&&GetPixel(target,48,48)==RGB(255,255,255));
  // Repeated exposure/drag paints copy the completed front without rebuilding
  // a bitmap, resampling, or submitting another GPU composition.
  for(int i=0;i<240;++i){assert(buffer.stage(target,image,v));assert(buffer.present(target,64,64));}
  assert(buffer.stages.count==1&&buffer.copies.count==241);
  v.width=128;assert(buffer.needs(v)&&buffer.stage(target,image,v)&&buffer.present(target,128,64));assert(GetPixel(target,32,16)==RGB(255,0,0)&&GetPixel(target,96,48)==RGB(255,255,255));
  v.dangerous=true;assert(buffer.needs(v)&&buffer.stage(target,image,v));
  auto invalid=std::make_shared<const std::vector<uint32_t>>(1,0xffffffffu);auto bad=v;bad.image=invalid;
  assert(!buffer.stage(target,invalid,bad)&&buffer.needs(bad));assert(buffer.present(target,128,64));assert(GetPixel(target,32,16)==RGB(255,0,0));
  auto replacement=std::make_shared<const std::vector<uint32_t>>(4096,0xff334455u);auto next=v;next.image=replacement;
  assert(buffer.stage(target,replacement,next)&&!buffer.needs(next)&&buffer.present(target,128,64));assert(GetPixel(target,32,16)==RGB(0x33,0x44,0x55));
  auto transparent=std::make_shared<const std::vector<uint32_t>>(4096,0u);next.image=transparent;assert(buffer.stage(target,transparent,next)&&buffer.present(target,128,64));assert(GetPixel(target,32,16)==RGB(0,0,0));
  buffer.clear();assert(buffer.needs(next)&&buffer.stage(target,transparent,next));
  watchpresentation::Gate gpu;auto key=v;key.gpu=true;key.sequence=1;
  assert(gpu.needs(key)); // failed/pending presentation does not commit
  assert(gpu.needs(key));gpu.committed(key);assert(!gpu.needs(key));++key.sequence;assert(gpu.needs(key));gpu.committed(key);key.dangerous=false;assert(gpu.needs(key));gpu.committed(key);gpu.invalidate();assert(gpu.needs(key));
  watchpanel::Buffer measured;auto benchmark=std::make_shared<const std::vector<uint32_t>>(320*256,0xff334455u);v.image=benchmark;v.sourceWidth=v.width=320;v.sourceHeight=v.height=256;v.dangerous=false;
  const double freshAt=watchpresentation::now();for(int i=0;i<240;++i){++v.sequence;assert(measured.stage(target,benchmark,v)&&measured.present(target,320,256));}const double freshMs=watchpresentation::now()-freshAt;const auto builds=measured.stages.count;
  const double heldAt=watchpresentation::now();for(int i=0;i<240;++i)assert(measured.stage(target,benchmark,v)&&measured.present(target,320,256));const double heldMs=watchpresentation::now()-heldAt;assert(measured.stages.count==builds);
  std::printf("PASS watch presentation: full-view resize, immutable identity, 240 exposures with zero extra rasters, failed stage preserves front/retries, transparent replacement clears, GPU pending gate. Synthetic 240 draws at 320x256: fresh %.3f ms, cached %.3f ms; raster builds %llu then +0.\n",freshMs,heldMs,(unsigned long long)builds);
  SelectObject(target,old);DeleteObject(bitmap);DeleteDC(target);
 }
 Gdiplus::GdiplusShutdown(token);
}
