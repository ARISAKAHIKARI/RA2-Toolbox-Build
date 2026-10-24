#include "../../cursor/source/core.h"
#include "../../cursor/source/super_hotspot.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using Image=std::vector<uint32_t>;
static void box(Image&im,int w,int x,int y,int rx,int ry,uint32_t color){
 for(int u=x-rx;u<=x+rx;++u)im[(y-ry)*w+u]=im[(y+ry)*w+u]=color;
 for(int v=y-ry;v<=y+ry;++v)im[v*w+x-rx]=im[v*w+x+rx]=color;
}
static Bytes file(const char*path){std::ifstream f(path,std::ios::binary);assert(f);return Bytes(std::istreambuf_iterator<char>(f),{});}
int main(int argc,char**argv){
 std::vector<Image> frames(450,Image(55*43));
 for(int n=346;n<351;++n){auto&f=frames[n];box(f,55,25,19,15,12,0xffff0000);box(f,55,25,19,10,8,0xffa00000);box(f,55,25,19,5,4,0xff700000);box(f,55,25,19,1,1,0xff700000);
  // Opaque shadow and off-centre animated decoration must not own the anchor.
  for(int x=2;x<53;++x)f[39*55+x]=0xff000000;
  f[(2+n%3)*55+50]=0xffff0000;
 }
 auto centers=superhot::prepare(frames,55,43);auto p=superhot::resolve(centers,57,346,346,5);
 assert(p.valid&&p.x==25.5&&p.y==19.5);
 for(int n=346;n<351;++n){auto q=superhot::resolve(centers,57,n,346,5);assert(q.valid&&q.x==p.x&&q.y==p.y);}
 assert(superhot::resolve(centers,-1,348,-1,0).valid); // Original frame is known; table snapshot temporarily unavailable.
 assert(superhot::resolve(centers,-1,348,347,2).valid); // Partial animation inference must not lose the verified art anchor.
 centers[53]={1,2,true};auto stale=superhot::resolve(centers,-1,348,53,5);assert(stale.valid&&stale.x==p.x&&stale.y==p.y); // Stale metadata from a previous cursor must not choose its anchor.
 assert(!superhot::resolve(centers,18,348,53,5).valid&&!superhot::resolve(centers,57,345,346,5).valid);
 for(int scale:{1,2,3,4}){
  assert(superhot::scaledAnchor(0,p,false,55,55*scale,13)==std::lround(p.x*scale));
  assert(superhot::scaledAnchor(0,p,true,43,43*scale,-21)==std::lround(p.y*scale));
  assert(superhot::scaledAnchor(9,{},false,55,55*scale,13)==9*scale+13);
 }
 frames[346].assign(55*43,0);frames[347].assign(55*43,0);frames[348].assign(55*43,0);
 assert(!superhot::resolve(superhot::prepare(frames,55,43),57,350,346,5).valid);
 assert(!superhot::ringCore(Image(10),55,43).valid);
 std::cout<<"PASS inner-ring anchor, stable animation, asymmetric shadow/decoration, unavailable table, conflicting cursor, scale/manual-offset precedence and unrecognized-art fallback\n";
 if(argc==3){
  Asset a=decode(file(argv[1]));Bytes palette=file(argv[2]);assert(palette.size()==1024);
  frames.assign(a.frames.size(),Image(size_t(a.w)*a.h));
  for(size_t n=0;n<a.frames.size();++n){const auto&f=a.frames[n];for(int y=0;y<f.h;++y)for(int x=0;x<f.w;++x){unsigned index=f.pixels[y*f.w+x];frames[n][size_t(f.y+y)*a.w+f.x+x]=index?u32(palette,index*4):0;}}
  auto real=superhot::prepare(frames,a.w,a.h);p=superhot::resolve(real,57,346,346,5);
  assert(p.valid&&p.x==28.5&&p.y==21.0);
  for(int n=346;n<351;++n){auto q=superhot::ringCore(frames[n],a.w,a.h);assert(q.valid&&q.x==p.x&&q.y==p.y);}
  std::cout<<"PASS supplied 450-frame SHA/palette: all five Iron Curtain frames share inner mark (28.5,21), independent of canvas/bounds\n";
 }
}
