#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <iostream>
struct Assets {
 struct ActorArt {uint32_t object=0;bool transient=false;};bool slow=false,expire=false;unsigned animationCaptures=0,unitCaptures=0;
 uint64_t revision(){return 1;}static size_t cost(const ActorArt&){return 1;}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xff556677u);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,external::voxel::Image&out){out={};out.width=60;out.height=30;out.x=-30;out.y=-15;out.pixels.assign(1800,0xff556677);out.flat=true;return true;}
 bool overlay(uint32_t,uint32_t,int,external::voxel::Image&,bool=false){return false;}
 bool shroud(bool,int,external::voxel::Image&){return false;}
 void buildingPose(uint32_t,uint32_t,external::visual::Pose&){}
 bool captureActor(uint32_t p,uint32_t,uint32_t,ActorArt&out){++unitCaptures;if(slow)std::this_thread::sleep_for(std::chrono::milliseconds(3));out={p,false};return true;}
 bool captureAnimation(uint32_t p,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&out){++animationCaptures;out={p,true};return !expire;}
 bool refreshActorPalette(ActorArt&a,bool&changed){changed=false;return !(expire&&a.transient);}
 bool renderActor(const ActorArt&a,const external::visual::Pose&,external::voxel::Image&out){out={};out.width=out.height=5;out.x=-2;out.y=-4;out.pixels.assign(25,a.transient?0xffff5500u:0xff335500u);out.writeDepth=false;return true;}
};
int main(){using namespace external;auto read=[](uint32_t,void*,size_t){return false;};
 auto t=std::make_shared<radar::Terrain>();t->geo={8,8,6,7,8,8,1,0x10000,0x20000,512*512,{8,8,0,0,8,8},8,8};t->reveal=true;t->colors.resize(512*512);t->art.resize(512*512);t->shade.resize(512*512);t->colors[7*512+7]=0x556677;t->art[7*512+7].type=1;t->art[7*512+7].sourceColor=0x556677;
 radar::Frame f;f.terrain=t;f.frame=1;f.tick=100;
 for(unsigned i=0;i<160;++i){radar::Marker m;m.object=0x20000+i*0x100;m.id=i+1;m.type=0x50000+i*0x100;m.kind=2;m.x=m.y=7*256;f.markers.push_back(m);}
 for(unsigned i=0;i<40;++i){auto m=f.markers[0];m.object=0x80000+i*0x100;m.id=500+i;m.kind=5;m.type=0x90000+i*0x100;f.markers.push_back(m);}
 Assets a;radar::SceneRenderer<Assets> renderer;auto scene=renderer.render(f,240,120,read,a);assert(scene&&a.animationCaptures==40&&a.unitCaptures>8);
 // Existing animations expire after the snapshot, before live palette refresh.
 a.expire=true;++f.tick;auto after=renderer.render(f,240,120,read,a);assert(after);
 f.markers.erase(std::remove_if(f.markers.begin(),f.markers.end(),[](const radar::Marker&m){return m.kind==5;}),f.markers.end());
 auto noTransients=renderer.render(f,240,120,read,a);assert(noTransients&&after->pixels==noTransients->pixels);
 Assets delayed;delayed.slow=true;radar::SceneRenderer<Assets> limited;assert(limited.render(f,240,120,read,delayed));assert(delayed.unitCaptures>0&&delayed.unitCaptures<=4);
 std::cout<<"PASS time-budget decoding admits more than 8 units; 40 animations bypass the ordinary unit queue; expired transients do not reject the world or retain stale pixels\n";
}
