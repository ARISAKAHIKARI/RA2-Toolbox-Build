#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/native_canvas.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <thread>
static std::atomic<long>failure{-1};static std::atomic<unsigned>injected{0};
void*operator new(std::size_t bytes){auto n=failure.load();if(n>=0&&failure.fetch_sub(1)==0){failure=-1;++injected;throw std::bad_alloc();}if(auto*p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}
using external::camera::NativeCanvas;
using external::voxel::Image;
static unsigned checks=0;static uint64_t pixels=0;
static uint32_t random(uint32_t n){n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;return n^(n>>16);}
static uint32_t reference(const NativeCanvas&c,double l,double t,double r,double b){
 double sum[3]{},area=(r-l)*(b-t);
 for(int y=int(std::floor(t));y<int(std::ceil(b));++y)for(int x=int(std::floor(l));x<int(std::ceil(r));++x){
  const double weight=(std::min(r,double(x+1))-std::max(l,double(x)))*(std::min(b,double(y+1))-std::max(t,double(y)));
  const uint32_t color=c.pixel(x,y);for(int ch=0;ch<3;++ch)sum[ch]+=weight*((color>>(16-ch*8))&255);
 }
 uint32_t out=0xff000000u;for(int ch=0;ch<3;++ch)out|=uint32_t(std::floor(sum[ch]/area+.5))<<(16-ch*8);return out;
}
static void compare(const NativeCanvas&c,int w,int h,double sx,double sy){
 std::vector<uint32_t>out;assert(c.present(w,h,out,w*sx,h*sy));
 for(int y=0;y<h;++y)for(int x=0;x<w;++x){assert(out[size_t(y)*w+x]==reference(c,x*sx,y*sy,(x+1)*sx,(y+1)*sy));++pixels;}++checks;
}
static NativeCanvas scene(int w,int h,uint32_t background){
 NativeCanvas c;assert(c.reset(w,h,{},background));Image tile;tile.width=173;tile.height=149;tile.skipZ=true;tile.pixels.resize(size_t(tile.width)*tile.height);
 for(size_t n=0;n<tile.pixels.size();++n)tile.pixels[n]=0xff000000u|(random(uint32_t(n))&0xffffffu);
 for(auto xy:{std::pair<int,int>{0,0},{127,119},{w-93,h-75}})c.blit(tile,float(xy.first),float(xy.second),1,1,0,{},0);
 return c;
}
static void fractions(){
 for(auto size:{std::pair<int,int>{315,170},{512,384},{769,515}}){auto c=scene(size.first,size.second,0x7f2163a4u);
  for(int x:{4,5,7,15,30,127,257})for(int y:{4,5,15,30,129}){const double sx=x*.5,sy=y*.5;int w=std::min(31,int(c.width()/sx)),h=std::min(25,int(c.height()/sy));if(w&&h)compare(c,w,h,sx,sy);}
  compare(c,73,29,314.25/73,169.0625/29); // arbitrary non-half scales keep the exact legacy filter
 }
 NativeCanvas sparse;assert(sparse.reset(32768,32768,{},0x7ffedcbau));std::vector<uint32_t>out;assert(sparse.present(1,1,out)&&out[0]==0xfffedcbau);++checks;
}
static void revisions(){auto c=scene(512,384,0xff102030u);std::vector<uint32_t>first,warm;assert(c.present(64,48,first));const auto prior=NativeCanvas::areaCacheStats();assert(c.present(64,48,warm)&&first==warm);const auto after=NativeCanvas::areaCacheStats();
 if(prior.boxBuilds)assert(prior.builds==after.builds);
 auto old=c.fork();c.clean();Image dot;dot.width=dot.height=1;dot.skipZ=true;dot.pixels={0xffffffff};for(auto xy:{std::pair<int,int>{127,127},{128,128},{511,383}})c.blit(dot,float(xy.first),float(xy.second),1,1,0,{},0);
 auto partial=first;assert(c.presentDirty(64,48,partial));assert(c.present(64,48,warm)&&partial==warm);std::vector<uint32_t>frozen;assert(old.present(64,48,frozen)&&frozen==first);compare(c,64,48,8,8);compare(c,64,48,7.5,7.5);compare(c,31,25,15,15);
 auto other=c.fork();std::vector<uint32_t>a,b;std::thread left([&]{for(int n=0;n<8;++n)assert(c.present(64,48,a,480,360));});std::thread right([&]{for(int n=0;n<8;++n)assert(other.present(31,25,b,465,375));});left.join();right.join();compare(c,64,48,7.5,7.5);compare(other,31,25,15,15);
}
static void inPlace(){auto c=scene(384,384,0xff334455u);Image dot;dot.width=dot.height=1;dot.skipZ=true;
 for(int frame=0;frame<12;++frame){dot.pixels={0xff000000u|(random(frame+19)&0xffffffu)};c.blit(dot,float(120+frame),127,1,1,0,{},0);compare(c,31,25,7.5,7.5);}
}
static void dirtySentinels(){auto c=scene(512,384,0xff334455u);Image dot;dot.width=dot.height=1;dot.skipZ=true;dot.pixels={0xffffffff};
 for(auto xy:{std::pair<int,int>{127,127},{128,128},{479,359}}){c.clean();c.blit(dot,float(xy.first),float(xy.second),1,1,0,{},0);std::vector<uint32_t>out(64*48,0x12345678u);assert(c.presentDirty(64,48,out,480,360));
  const int l=int(std::floor(xy.first/7.5)),r=std::min(64,int(std::ceil((xy.first+1)/7.5))),t=int(std::floor(xy.second/7.5)),b=std::min(48,int(std::ceil((xy.second+1)/7.5)));
  for(int y=0;y<48;++y)for(int x=0;x<64;++x){const bool dirty=x>=l&&x<r&&y>=t&&y<b;assert(out[size_t(y)*64+x]==(dirty?reference(c,x*7.5,y*7.5,(x+1)*7.5,(y+1)*7.5):0x12345678u));}++checks;
  c.clean();auto unchanged=out;assert(c.presentDirty(64,48,out,480,360)&&out==unchanged);
 }
}
static void failures(){
 for(long allocation=0;allocation<48;++allocation){auto c=scene(256,192,0xff112233u);std::vector<uint32_t>expected,actual(16*12);assert(c.present(16,12,expected));
  // Change geometry so every optional table is new. Out storage is already
  // allocated; a single failure belongs to the optional preparation path.
  assert(c.present(16,12,expected,240,180));failure=allocation;bool ok=c.present(16,12,actual,256,192);failure=-1;assert(ok);
  for(int y=0;y<12;++y)for(int x=0;x<16;++x)assert(actual[size_t(y)*16+x]==reference(c,x*16.,y*16.,(x+1)*16.,(y+1)*16.));++checks;
 }
}
int main(){const auto before=NativeCanvas::areaCacheStats().bytes;fractions();revisions();inPlace();dirtySentinels();if(NativeCanvas::areaCacheStats().boxBuilds)failures();assert(NativeCanvas::areaCacheStats().bytes==before);std::printf("PASS exact box reduction: %u cases, %llu scalar pixel oracles; sparse/partial pages, half pixels, COW, dirty-only writes, in-place revisions, two concurrent scales,%u injected optional allocation failures and complete cache-credit retirement\n",checks,static_cast<unsigned long long>(pixels),injected.load());}
