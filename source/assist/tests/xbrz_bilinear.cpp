#include "../../external_reader/source/xbrz_adapter.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <random>
using namespace external::xbrzadapter;
static uint32_t reference(uint32_t a,uint32_t b,uint32_t c,uint32_t d,double x,double y){
 uint32_t result=0;
 for(int s:{0,8,16,24}){
  const double top=double((a>>s)&255)*(1-x)+double((b>>s)&255)*x;
  const double bottom=double((c>>s)&255)*(1-x)+double((d>>s)&255)*x;
  result|=uint32_t(std::floor(top*(1-y)+bottom*y+.5))<<s;
 }
 return result;
}
static uint32_t rgba(std::mt19937&rng){
 unsigned a=rng()&255,r=(rng()&255)*a/255,g=(rng()&255)*a/255,b=(rng()&255)*a/255;
 return a<<24|r<<16|g<<8|b;
}
int main(){
 std::mt19937 rng(62781);int maximumError=0;size_t cases=0;
 auto check=[&](uint32_t a,uint32_t b,uint32_t c,uint32_t d,double fx,double fy){
  unsigned wx=unsigned(fx*256+.5),wy=unsigned(fy*256+.5);
  uint32_t scalar=bilinearPacked(a,b,c,d,wx,wy),fast=bilinearResolve(a,b,c,d,wx,wy);
  assert(scalar==fast);const uint32_t expected=reference(a,b,c,d,fx,fy);
  for(int s:{0,8,16,24}){int error=std::abs(int((fast>>s)&255)-int((expected>>s)&255));maximumError=std::max(maximumError,error);assert(error<=1);}
  for(int s:{0,8,16})assert(((fast>>s)&255)<=(fast>>24));++cases;
 };
 // Every quantized x/y weight, plus tie-neighbor real coordinates, protects
 // against an extra intermediate round and unsigned/signed SIMD mistakes.
 for(unsigned y=0;y<=256;++y)for(unsigned x=0;x<=256;++x){
  auto a=rgba(rng),b=rgba(rng),c=rgba(rng),d=rgba(rng);check(a,b,c,d,x/256.,y/256.);
 }
 for(int n=0;n<250000;++n){
  auto a=rgba(rng),b=rgba(rng),c=rgba(rng),d=rgba(rng);
  double x=(rng()%1000001)/1000000.,y=(rng()%1000001)/1000000.;check(a,b,c,d,x,y);
 }
 for(double v:{0.,.5/256-1e-12,.5/256,.5/256+1e-12,.5,1.-.5/256,1.}){
  check(0,0xffffffff,0xffffffff,0,v,1-v);
  check(0xffffffff,0,0,0xffffffff,v,v);
  check(0x80800000,0x80008000,0x80000080,0x80808080,v,1-v);
 }
#ifdef RA2_XBRZ_SCALAR_ONLY
 assert(!bilinearSse2Available());
#endif
 // The actual 4K/1.5x viewport needs 66 source tiles: 16M holds only 61.
 // The bounded larger budget must avoid five unnecessary xBRZ rebuilds on
 // every warm frame, then shrink again when returning to a small viewport.
 RegionCache cache;std::vector<uint32_t> out;size_t reads=0;
 auto fetch=[&](int x,int y){++reads;return ((x/16+y/16)&1)?0xff70a040u:0xff456789u;};
 assert(cache.sampleRegion(fetch,10.25,13.5,2560,1440,3840,2160,out));
 assert(cache.budgetPixels()>MaxPixels&&cache.budgetPixels()<=24*1024*1024);
 assert(cache.residentPixels()>MaxPixels&&cache.residentPixels()<=cache.budgetPixels());
 const auto builds=cache.builds();reads=0;
 auto start=std::chrono::steady_clock::now();
 assert(cache.sampleRegion(fetch,11.25,13.5,2560,1440,3840,2160,out));
 const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 assert(cache.builds()==builds&&reads==0);
 assert(cache.sampleRegion(fetch,20,20,4,4,16,16,out));
 assert(cache.budgetPixels()==MaxPixels&&cache.residentPixels()<=MaxPixels);
 std::printf("PASS: %zu bilinear cases, SIMD/scalar byte equality, independent double max error=%d, premultiplied alpha; 4K1.5x warm %.3fms, no xBRZ rebuilds, cache 16M..24M then shrinks (SSE2=%d)\n",cases,maximumError,elapsed,int(bilinearSse2Available()));
}
