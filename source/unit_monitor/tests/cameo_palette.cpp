#include "../source/cameo_reader.h"
#include <cassert>
#include <cstring>
#include <iostream>
static std::vector<uint8_t> asset;
static int reads=0,loads=0;
static bool file(const char*name,std::vector<uint8_t>&out){
 ++loads;assert(!strcmp(name,"cameo.pal"));out=asset;return !out.empty();
}
int main(){
 um::Snapshot s{};auto unavailable=[](uint32_t,void*,size_t){++reads;return false;};
 asset.resize(768);asset[3]=63;asset[4]=32;asset[5]=1;
 assert(um::readCameoPalette(unavailable,s,file)&&s.paletteReady&&reads==0&&loads==1);
 assert(s.palette[0]==0&&s.palette[1]==0xfffc8004);
 // Invalid/missing file must preserve the native 16-bit palette fallback.
 asset[0]=64;uint16_t palette[256]{};palette[1]=0xf800;
 auto native=[&](uint32_t p,void*out,size_t n){++reads;uint32_t value=p==0x87f6b0?0x10000:p==0x10004?2:p==0x10174?0x20000:0;
  if(p==0x20000&&n==sizeof palette){memcpy(out,palette,n);return true;}
  if(value&&n==4){memcpy(out,&value,n);return true;}return false;};
 s={};assert(um::readCameoPalette(native,s,file)&&s.palette[1]==0xffff0000);
 asset.resize(767);s={};assert(um::readCameoPalette(native,s,file));
 asset.clear();s={};assert(!um::readCameoPalette(unavailable,s,file)&&!s.paletteReady);
 std::cout<<"PASS common building/statistics palette: MIX file with no native converter, 6-bit colors, transparency, malformed/missing file fallback\n";
}
