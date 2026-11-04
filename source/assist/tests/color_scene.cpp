#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <cstdio>
struct Assets {
 struct ActorArt{};uint64_t revision(){return 1;}static size_t cost(const ActorArt&){return 1;}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xffffffff);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,external::voxel::Image&i){i={};i.width=60;i.height=30;i.x=-30;i.y=-15;i.pixels.assign(1800,0xffffffff);i.indices.assign(1800,10);i.writeDepth=false;return true;}
 bool overlay(uint32_t,uint32_t,int,external::voxel::Image&,bool=false){return false;}
 bool shroud(bool,int,external::voxel::Image&){return false;}
 void buildingPose(uint32_t,uint32_t,external::visual::Pose&){}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&){return true;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool renderActor(const ActorArt&,const external::visual::Pose&,external::voxel::Image&i){i={};i.width=i.height=2;i.pixels.assign(4,0xffffffff);i.indices.assign(4,10);i.objectPalette=true;i.writeDepth=false;return true;}
};
int main(){
 using namespace external::radar;Assets assets;SceneRenderer<Assets> renderer;
 auto terrain=std::make_shared<Terrain>();terrain->geo={8,8,6,7,8,8,1,0x10000,0x20000,512*512,{8,8,0,0,8,8},8,8};terrain->reveal=true;terrain->colors.resize(512*512);terrain->art.resize(512*512);terrain->shade.resize(512*512);terrain->colors[7*512+7]=0x202020;terrain->art[7*512+7].type=1;
 Frame frame;frame.terrain=terrain;frame.frame=42;frame.tick=100;frame.posesPrepared=true;
 Marker m;m.x=m.y=7*256;m.object=0x30000;m.id=1;m.type=0x40000;m.owner=0x20000;m.kind=2;m.layer=3;frame.markers={m};
 auto read=[](uint32_t p,void*out,size_t n){if(n!=4)return false;uint32_t v=0;if(p==0xA8B230)v=0x50000;else if(p==0x50000+0x352C)v=50;else if(p>=0x50000+0x3534&&p<0x50000+0x3540)v=100;else return false;std::memcpy(out,&v,4);return true;};
 // lighting::sample reads the RGB triplet as one transaction.
 auto memory=[&](uint32_t p,void*out,size_t n){if(p==0x50000+0x3534&&n==12){int tint[3]={100,100,100};std::memcpy(out,tint,12);return true;}return read(p,out,n);};
 auto legacy=renderer.render(frame,240,120,memory,assets,{0,0,240,120,true},0,true,true,false);assert(legacy);
 auto enhanced=renderer.render(frame,240,120,memory,assets,{0,0,240,120,true},0,true,true,true);assert(enhanced);
 assert(legacy->pixels[105*240+180]==0xff7b7b7b&&enhanced->pixels[105*240+180]==0xff808080);
 assert(legacy->pixels[110*240+190]==0xff7b7b7b&&enhanced->pixels[110*240+190]==0xff808080); // native terrain pages use same mode
 auto restored=renderer.render(frame,240,120,memory,assets,{0,0,240,120,true},0,true,true,false);assert(restored&&restored->pixels==legacy->pixels);
 std::puts("PASS: live SceneRenderer color precision reaches both native terrain and physical actor canvas; mode switch invalidates old light pixels and restores compatibility");
}
