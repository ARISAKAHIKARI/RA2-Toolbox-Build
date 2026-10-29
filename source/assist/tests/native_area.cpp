#include "../../external_reader/source/native_canvas.h"
#include "../../external_reader/source/gpu_area.h"
#include <cassert>
#include <cstdio>
#include <random>
using namespace external;
static gpuarea::Plan plan(int nw,int nh,int w,int h,std::mt19937&r){
 gpuarea::Plan p;p.nativeWidth=nw;p.nativeHeight=nh;p.width=w;p.height=h;p.spanX=nw-.375;p.spanY=nh-.625;p.background=0x123d2811;if(nw==2688){p.spanX=nw;p.spanY=nh;}
 for(int y=0;y<(nh+127)/128;++y)for(int x=0;x<(nw+127)/128;++x)if((x+2*y)%4!=2){auto v=std::make_shared<std::vector<uint32_t>>(16384);for(auto&pixel:*v)pixel=0xff000000u|(r()&0xffffff);p.pages.push_back({uint32_t(x),uint32_t(y),v});}return p;
}
int main(){std::mt19937 random(20261029);uint64_t checked=0;unsigned maxError=0;
 for(auto shape:std::vector<std::array<int,4>>{{257,259,73,51},{511,389,31,17},{128,128,1,1},{513,275,509,271},{19,13,79,61},{2688,128,19,3},{513,128,23,519},{128,513,519,23}}){auto p=plan(shape[0],shape[1],shape[2],shape[3],random);camera::NativeCanvas canvas;assert(canvas.reset(p.nativeWidth,p.nativeHeight,{},p.background));
  for(const auto&page:p.pages){voxel::Image image;image.width=image.height=128;image.flat=true;image.writeDepth=false;image.pixels=*page.pixels;canvas.blit(image,page.x*128,page.y*128,1,1,0,lighting::State{});}
  std::vector<uint32_t>expected,actual;assert(canvas.present(p.width,p.height,expected,p.spanX,p.spanY));assert(gpuarea::reference(p,actual));assert(expected.size()==actual.size());
  for(size_t n=0;n<actual.size();++n)for(int shift:{0,8,16,24}){unsigned error=unsigned(std::abs(int((actual[n]>>shift)&255)-int((expected[n]>>shift)&255)));maxError=std::max(maxError,error);assert(error<=1);++checked;}
  auto before=actual;auto invalid=p;invalid.nativeWidth=0;assert(!gpuarea::reference(invalid,actual)&&before==actual);invalid=p;invalid.pages.push_back(invalid.pages.front());assert(!gpuarea::reference(invalid,actual)&&before==actual);
 }
 camera::NativeCanvas exact;assert(exact.reset(128,128,{},0x12345678));exact.underlayRect(2,2,4,4,0x112233);gpuarea::Plan native;assert(exact.areaPlan(128,128,native));std::vector<uint32_t>a,b;assert(exact.present(128,128,a)&&gpuarea::reference(native,b)&&a==b);checked+=a.size()*4;
 std::printf("PASS sparse native area oracle channels=%llu max_error=%u; fractional spans, page edges/holes, large shrink, upscale, opaque-RGB contract and invalid-plan output preservation\n",(unsigned long long)checked,maxError);
}
