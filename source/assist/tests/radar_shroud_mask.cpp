#include "../../external_reader/source/radar_shroud_mask.h"
#include <cassert>
#include <iostream>
using namespace external;

static bool sprite(bool fog,int frame,voxel::Image&out){
 out={};out.canvasWidth=60;out.canvasHeight=30;out.width=60;out.height=30;out.x=-30;out.y=-15;
 out.indices.assign(1800,0xfe);
 for(int y=0;y<30;++y)for(int x=0;x<60;++x)if(std::abs(x-29.5)/30.+std::abs(y-14.5)/15.<1.){
  out.indices[y*60+x]=uint8_t(fog?64:frame==15?2:frame==4?(x<30?2:127):127);
 }
 return true;
}
int main(){
 radar::Terrain t;t.geo={64,64,64,64,128,128,1,0x10000,0x20000,512*512,{64,64,0,0,64,64},128,128};
 t.shade.resize(512*512);const size_t clear=64*512+64,hidden=64*512+65;
 t.shade[clear]={-1,-1};t.shade[hidden]={-2,-2};
 mapbounds::TileOrigin ground{},mask{};assert(t.geo.tileOrigin(64,64,2,ground));assert(t.geo.shroudOrigin(64,64,mask));
 assert(ground.x==mask.x&&ground.y==mask.y-30); // Level moves only art.
 radar::shroudmask::Raster raster;
 assert(raster.build(t,384,192,3840,1920,false,sprite));
 assert(raster.alpha[97*384+192]==127); // cell centre at native (1920,975)
 assert(raster.alpha[99*384+195]==2);   // adjacent opaque cell at (1950,990)
 assert(raster.alpha[97*384+188]<127);  // the world is covered beyond the left edge
 auto original=raster.alpha;t.art.resize(512*512);t.art[clear].level=11;
 assert(raster.build(t,384,192,3840,1920,false,sprite)&&raster.alpha==original);
 std::vector<uint32_t> pixels(size_t(384)*192,0xffc86432u);raster.apply(pixels);
 assert(pixels[97*384+192]==0xffc86432u);
 assert((pixels[99*384+195]&0xffffffu)<0x040404u); // actor/repair spill is hidden after composition
 // A partial edge frame is taken from the SHP pixel values, not a Boolean
 // exploration flag; fog combines with that raw alpha after the shroud.
 t.shade[clear]={4,4};t.shade[hidden]={127,127};
 assert(raster.build(t,384,192,3840,1920,false,sprite));
 assert(raster.alpha[97*384+190]==2&&raster.alpha[97*384+193]==127);
 assert(raster.build(t,384,192,3840,1920,true,sprite));
 assert(raster.alpha[97*384+193]==64&&raster.alpha[97*384+190]==0);
 t.reveal=true;assert(!raster.build(t,384,192,3840,1920,true,sprite));
 // Exact 1:1 source pixels and a strongly reduced map both use the same
 // native frame coordinates. The reduced pixel averages twelve offset source
 // samples; choosing only its centre would incorrectly yield clear (127).
 t.reveal=false;t.geo.offsetX=2;t.geo.offsetY=128;t.geo.rawWidth=8;t.geo.rawHeight=6;
 t.shade.assign(512*512,{});t.shade[clear]={4,-1};
 assert(raster.build(t,240,90,240,90,false,sprite));
 assert(raster.alpha[15*240+55]==2&&raster.alpha[15*240+65]==127);
 assert(raster.build(t,6,3,240,90,false,sprite));
 assert(raster.alpha[1]==44);
 std::cout<<"PASS native flat shroud anchor, asymmetric edge frames, fog alpha, height independence and post-composition masking\n";
}
