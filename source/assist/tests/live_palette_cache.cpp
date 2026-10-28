#include "../../external_reader/source/live_palette_cache.h"
#include "../../external_reader/source/terrain_preview.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace external::livepalette;std::array<uint32_t,256> palette{};palette[3]=0xff123456;bool live=true,changed=false;uint64_t revision=4;
 Sample source;source.colors=palette;source.live=true;int reads=0;
 auto stable=[&](Sample&out){++reads;out=source;return true;};
 assert(refresh(palette,live,revision,changed,stable)&&!changed&&revision==4&&reads==2);
 source.colors[3]=0xffabcdef;assert(refresh(palette,live,revision,changed,stable)&&changed&&revision==5&&palette[3]==0xffabcdef);
 auto coherent=palette;int sequence=0;
 auto changing=[&](Sample&out){out=source;out.colors[3]+=++sequence;return true;};
 assert(!refresh(palette,live,revision,changed,changing)&&!changed&&palette==coherent&&revision==5);
 auto failed=[](Sample&){return false;};assert(!refresh(palette,live,revision,changed,failed)&&palette==coherent);
 source.live=false;source.colors[3]=0xff000000;assert(!refresh(palette,live,revision,changed,stable)&&palette==coherent&&live);
 live=false;assert(refresh(palette,live,revision,changed,stable)&&changed&&revision==6);
 source.live=true;assert(refresh(palette,live,revision,changed,stable)&&!changed&&live&&revision==6);
 std::array<uint8_t,768> rgb{};rgb.fill(70);unsigned calls=0;bool tear=false,unreadable=false;
 auto terrainRead=[&](uint32_t address,void*out,size_t n){assert(address==0x10000&&n==768);++calls;if(unreadable&&calls%2==0)return false;std::memcpy(out,rgb.data(),n);if(tear&&calls%2==0)static_cast<uint8_t*>(out)[5]++;return true;};
 assert(external::terrain::palette(terrainRead,0x10000,palette)&&palette[8]==0xff464646&&calls==2);
 coherent=palette;tear=true;assert(!external::terrain::palette(terrainRead,0x10000,palette)&&palette==coherent);tear=false;unreadable=true;assert(!external::terrain::palette(terrainRead,0x10000,palette)&&palette==coherent);
 std::puts("PASS: same-pointer palette content refresh, unchanged reuse, torn/unavailable rejection, live-source fallback guard and per-art revisions");
}
