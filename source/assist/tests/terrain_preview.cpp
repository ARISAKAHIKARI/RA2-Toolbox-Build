#include "../../external_reader/source/terrain_preview.h"
#include <cassert>
#include <iostream>
using namespace external;
int main(){
 std::vector<uint8_t> memory(0x40000);auto word=[&](uint32_t at,uint32_t v){memcpy(memory.data()+at,&v,4);};
 auto read=[&](uint32_t at,void*p,size_t n){if(uint64_t(at)+n>memory.size())return false;memcpy(p,memory.data()+at,n);return true;};
 uint32_t base=0x10000,block=0x12000;word(base,2);word(base+4,2);word(base+8,60);word(base+12,30);word(base+16+2*4,block);
 word(block,uint32_t(-30));word(block+4,15);word(block+8,1852);word(block+12,952);word(block+20,uint32_t(-40));word(block+24,3);word(block+28,20);word(block+32,12);word(block+36,1);
 std::fill(memory.begin()+block+52,memory.begin()+block+952,1);std::fill(memory.begin()+block+1852,memory.begin()+block+1852+240,2);memory[block+1852]=0;
 std::array<uint32_t,256> palette{};palette[1]=0xff123456;palette[2]=0xffabcdef;
 voxel::Image image;assert(terrain::tile(read,base,2,palette,image));assert(image.width==70&&image.height==42&&image.x==-40&&image.y==-27);
 assert(std::count(image.pixels.begin(),image.pixels.end(),palette[1])==900);assert(std::count(image.pixels.begin(),image.pixels.end(),palette[2])==239);assert(image.pixels[0]==0);
 // Palette bytes are already expanded by 5454D6; don't shift them twice.
 memory[0x20000+3]=200;memory[0x20000+4]=100;memory[0x20000+5]=44;std::array<uint32_t,256> colors{};assert(terrain::palette(read,0x20000,colors)&&colors[1]==0xffc8642c);
 word(block+36,0);assert(terrain::tile(read,base,2,palette,image));assert(image.width==60&&image.height==30&&std::count(image.pixels.begin(),image.pixels.end(),palette[1])==900);
 // No extra image: these fields are unused by the native renderer.
 word(block+36,0x80000000);word(block+20,0xcdcdcdcd);word(block+24,0xcdcdcdcd);
 assert(terrain::tile(read,base,2,palette,image));word(block+36,0);word(block+20,uint32_t(-40));word(block+24,3);
 assert(terrain::tile(read,base,6,palette,image));assert(!terrain::tile(read,base,4,palette,image));word(base+16+2*4,0);assert(!terrain::tile(read,base,2,palette,image));word(base+16+2*4,block);
 word(block+12,0);assert(!terrain::tile(read,base,2,palette,image));word(block+12,952);word(block+36,1);word(block+28,513);assert(!terrain::tile(read,base,2,palette,image));word(block+28,20);word(block+8,0xfffffff0);assert(!terrain::tile(read,base,2,palette,image));word(block+8,1852);
 int calls=0;auto racing=[&](uint32_t at,void*p,size_t n){bool ok=read(at,p,n);if(at==base+24&&++calls==2)*static_cast<uint32_t*>(p)=block+4;return ok;};assert(!terrain::tile(racing,base,2,palette,image));
 // SHP frame selection is shared by tree animation and cell overlays.
 uint32_t shp=0x24000;uint16_t h[]={0,2,1,2};memcpy(memory.data()+shp,h,sizeof h);
 for(int n=0;n<2;++n){uint16_t f[]={0,0,2,1};memcpy(memory.data()+shp+8+n*24,f,8);word(shp+8+n*24+20,56+n*2);memory[shp+56+n*2]=n+1;memory[shp+57+n*2]=n+1;}
 assert(voxel::sprite(read,shp,palette,image,1));assert(image.pixels[0]==voxel::premultiply(palette[2],1));assert(!voxel::sprite(read,shp,palette,image,2));
 std::cout<<"PASS relocated TMP directory, 900-pixel diamond, cliff extension/origin/transparency, expanded palette, SHP frame selection, bounds and concurrent replacement\n";
}
