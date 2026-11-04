#include "../../external_reader/source/native_palette_lookup.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace external;uint64_t checks=0;
 for(bool format:{false,true})for(unsigned word=0;word<65536;++word){assert(nativesurface::packedRGB(format)[word]==nativepacked::color(word,0,format));++checks;}
 for(unsigned rows:{1u,2u,17u,53u,256u})for(int brightness:{-1,0,1,250,1000,1500,2000,3000})
 for(bool scalar:{false,true})for(bool special:{false,true})for(bool format:{false,true})for(bool opaque:{false,true})for(bool table:{false,true}){
  nativesurface::Palette p;p.rows=rows;p.brightness=brightness;p.scale={{78123,48765,110001}};p.scalar=scalar;p.special=special;p.rgb555=format;p.colour=0x8421;
  if(table){auto colors=std::make_shared<std::vector<uint32_t>>(size_t(rows)*256);for(size_t n=0;n<colors->size();++n)(*colors)[n]=uint32_t(n*1789+0x8123);p.table=colors;}
  nativesurface::PaletteLookup lookup;lookup.prepare(p,opaque);assert(lookup.table()==table);
  for(unsigned a=0;a<256;++a)for(unsigned index:{0u,1u,239u,240u,254u,255u})for(uint32_t pixel:{0xff123456u,0x7c806040u,0x00345678u}){
   assert(lookup.apply(pixel,uint8_t(index),uint8_t(a))==p.apply(pixel,uint8_t(index),a,opaque));++checks;
  }
 }
 // An active blit keeps its immutable math alive while the bounded cache churns.
 nativesurface::Palette original;original.rows=53;original.brightness=1100;original.scale={{60001,70001,80001}};
 nativesurface::PaletteLookup held;held.prepare(original,true);
 for(unsigned n=0;n<5000;++n){nativesurface::Palette p;p.scale={{n+1000,n+2000,n+3000}};nativesurface::PaletteLookup other;other.prepare(p,false);}
 for(unsigned a=0;a<256;++a){assert(held.apply(0xff345678,244,uint8_t(a))==original.apply(0xff345678,244,a,true));++checks;}
 std::printf("PASS: %llu exact palette/packed-color comparisons; full A range, native-table/fallback, 555/565, shade counts, clamp, emissive and cache lifetime; no GPU\n",(unsigned long long)checks);
}
