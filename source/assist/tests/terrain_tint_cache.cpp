#include "../../external_reader/source/scene_lighting.h"
#include <cassert>
#include <cstdio>
using namespace external;
int main(){
 unsigned reads=0;int tint=1000;bool fail=false;
 auto read=[&](uint32_t p,void*out,size_t n){++reads;
  if(p==0x30000+0x1B0&&n==1){*static_cast<uint8_t*>(out)=0;return !fail;}
  if(p==0x30000+0x198&&n==12){int v[3]={tint,1000,1000};std::memcpy(out,v,12);return !fail;}
  uint32_t offset=p&0xfff;if(offset==0x104&&n==4){uint32_t v=1;std::memcpy(out,&v,4);return true;}
  if(offset==0x34&&n==4){uint32_t v=0x30000;std::memcpy(out,&v,4);return true;}
  if(offset==0x10A&&n==6){int16_t v[3]={1000,700,900};std::memcpy(out,v,6);return true;}return false;
 };
 lighting::TintCache cache;lighting::State baseline;
 for(int i=0;i<4096;++i){auto a=lighting::cell(read,0x40000+i*0x1000,baseline,lighting::Layer::Terrain,true,&cache);assert(a.intensity==700&&a.tint[0]==1000);}
 const unsigned cached=reads;assert(cached==4096*4+4);
 reads=0;for(int i=0;i<4096;++i)lighting::cell(read,0x40000+i*0x1000,baseline,lighting::Layer::Terrain);assert(reads==4096*8);
 tint=450;lighting::TintCache next;assert(lighting::cell(read,0x40000,baseline,lighting::Layer::Terrain,true,&next).tint[0]==450);
 fail=true;lighting::TintCache invalid;baseline.tint[0]=123;assert(lighting::cell(read,0x40000,baseline,lighting::Layer::Terrain,true,&invalid).tint[0]==123);baseline.tint[0]=456;assert(lighting::cell(read,0x40000,baseline,lighting::Layer::Terrain,true,&invalid).tint[0]==456);
 std::printf("PASS 4096 lit cells: %u -> %u reader calls, next-epoch tint changes and per-call fallback preserved\n",4096*8,cached);
}
