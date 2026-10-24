#include "../../external_reader/source/building_animations.h"
#include "../../external_reader/source/shape_depth.h"
#include "../../external_reader/source/sprite_names.h"
#include <cassert>
#include <iostream>
int main(){
 std::vector<uint8_t> mem(0x40000);auto read=[&](uint32_t p,void*out,size_t n){if(uint64_t(p)+n>mem.size())return false;memcpy(out,mem.data()+p,n);return true;};
 auto put=[&](uint32_t p,auto v){memcpy(mem.data()+p,&v,sizeof v);};
 uint32_t b=0x10000,a=0x18000,t=0x20000,bt=0x28000;put(b,0x7E3EBCu);put(b+0x10,42u);put(b+0x6C,100);put(b+0x21C,0x30000u);put(b+0x520,bt);put(b+0x9C,2560);put(b+0xA0,5120);put(b+0xA4,208);
 put(b+0x55C,a);put(b+0x5C8,a);put(a,0x7E3354u);put(a+0x10,77u);put(a+0x90,uint8_t(1));put(a+0xC8,t);put(a+0xCC,0u);put(a+0x9C,2432);put(a+0xA0,4992);put(a+0xA4,208);put(a+0xAC,6);put(a+0x100,-32);put(a+0x104,362);put(t+0x35D,uint8_t(1));put(t+0x364,2);
 auto states=external::animations::attached(read,b,42);assert(states.size()==1&&states[0].x==2432&&states[0].y==4992&&states[0].z==208&&states[0].frame==6&&states[0].zAdjust==-32&&states[0].normalLight&&states[0].sort==362&&states[0].layer==2);
 put(a+0xAC,0);states=external::animations::attached(read,b,42);assert(states.size()==1&&states[0].frame==0);
 put(a+0x11A,uint8_t(1));put(a+0x11C,3);put(a+0xAC,9);put(t+0x2B4,2);states=external::animations::attached(read,b,42);assert(states.size()==1&&states[0].frame==11); // native Draw uses Value + Start even when paused
 put(a+0x19E,uint8_t(1));put(a+0xAC,0);put(t+0x2B4,0);states=external::animations::attached(read,b,42);assert(states.size()==1&&states[0].frame==0); // powered-off / neutral pump
 put(a+0x19D,uint8_t(1));assert(external::animations::attached(read,b,42).empty());put(a+0x19D,uint8_t(0));
 put(a+0xAC,9000);assert(external::animations::attached(read,b,42).empty());put(a+0xAC,3);
 put(a+0xCC,b+4);assert(external::animations::attached(read,b,42).empty());put(a+0xCC,0u);
 assert(external::animations::attached(read,b,41).empty());put(a+0x9C,132000);assert(external::animations::attached(read,b,42).empty());put(a+0x9C,100000);assert(external::animations::attached(read,b,42).size()==1); // valid far side of a large map
 external::animations::State global;assert(external::animations::capture(read,a,0,0,global)&&global.parent==0&&global.frame==3);
 put(a+0x179,uint8_t(1));assert(!external::animations::capture(read,a,0,0,global));put(a+0x179,uint8_t(0));
 // A real SetOwnerObject attachment is relative to the building CENTER; this
 // native foundation table read is deliberately separate from the slot link.
 put(a+0xCC,b);put(a+0x9C,128);put(a+0xA0,-128);put(a+0xA4,64);put(bt+0xEF0,3);
 auto dimensions=[&](uint32_t p,void*out,size_t n){if(n==4&&(p==0x8192B8+12||p==0x819310+12)){int v=2;memcpy(out,&v,4);return true;}return read(p,out,n);};
 states=external::animations::attached(dimensions,b,42);assert(states.size()==1&&states[0].x==2816&&states[0].y==5120&&states[0].z==272);
 put(a+0xCC,0u);put(a+0x9C,2432);put(a+0xA0,4992);
 put(b+0x55C,0u);put(b+0x5C8,0u);external::animations::State state;assert(!external::animations::capture(read,a,b,42,state));put(b+0x55C,a);
 int count=0;auto race=[&](uint32_t p,void*out,size_t n){if(p==a+0x10&&++count==2)put(a+0x10,78u);return read(p,out,n);};assert(!external::animations::capture(race,a,b,42,state));put(a+0x10,77u);
 auto removed=[&](uint32_t p,void*out,size_t n){if(p==b+0x55C&&n==4)put(b+0x55C,0u);return read(p,out,n);};assert(!external::animations::capture(removed,a,b,42,state));
 external::voxel::Image mask,image;mask.canvasWidth=mask.width=396;mask.canvasHeight=mask.height=477;mask.x=-198;mask.y=-238;mask.indices.assign(396*477,66);
 image.width=4;image.height=2;image.pixels.assign(8,0xffffffff);mask.indices[446*396+198]=69;
 external::camera::buildingDepth(image,mask,1,1,0,0);assert(image.depth[0]==-8&&image.depth[1]==-11);
 external::camera::buildingDepth(image,mask,1,1,1,0);assert(image.depth[0]==-11);
 auto diskDepth=image.depth;for(auto&v:mask.indices)if(v)v=uint8_t(v-65);
 external::camera::buildingDepth(image,mask,1,1,1,0,true);assert(image.depth==diskDepth);
 // Signed native mask values must retain negative depth, not wrap to 200+.
 mask.indices[446*396+199]=uint8_t(-10);external::camera::buildingDepth(image,mask,1,1,1,0,true);assert(image.depth[0]==-22);
 for(const auto&climate:std::vector<std::pair<std::string,std::string>>{{"TEM","T"},{"SNO","A"},{"URB","U"},{"LUN","L"},{"DES","D"},{"UBN","N"}}){
  for(const auto&stem:std::vector<std::string>{"CAOILD_A","GACNST_A","NACNST_B","YACNST_A"}){
   auto names=external::assets::spriteNames(stem,"",climate.first,climate.second,false,true,false,true);
   auto current=stem;current[1]=climate.second[0];assert(names.front()==current+".SHP");
   auto generic=stem;generic[1]='G';assert(std::find(names.begin(),names.end(),generic+".SHP")!=names.end());
   assert(std::find(names.begin(),names.end(),stem+".SHP")!=names.end());assert(names.size()<=3);
  }
 }
 auto names=external::assets::spriteNames("CAOILD_A","MODPUMP.SHP","SNO","A",false,true,false,true);assert(names.front()=="MODPUMP.SHP");
 names=external::assets::spriteNames("PUMP","","SNO","A",true,false,false,true);assert((names==std::vector<std::string>{"PUMP.SNO","PUMP.SHP"}));
 // No foreign theater may win simply because its archive is installed.
 names=external::assets::spriteNames("CAOILD_A","","SNO","A",false,true,false,true);
 assert(std::find(names.begin(),names.end(),"CTOILD_A.SHP")==names.end());
 std::cout<<"PASS attached animation slots, absolute/attached coordinates, native Value+Start and layer/YSort, hidden/dead/recycled guards, six-theater oil/yard resource selection and building Z-mask\n";
}
