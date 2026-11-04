#ifndef RADAR_SCENE_HEADER
#define RADAR_SCENE_HEADER "../../external_reader/source/radar_scene.h"
#endif
#include RADAR_SCENE_HEADER
#include <cassert>
#include <cstdio>
#include <set>
struct Assets {
 static constexpr uint32_t Yard=0x77700;
 struct ActorArt{uint32_t type=0;int role=99;};bool unavailable=false;unsigned yardCalls=0;std::set<int>frames;
 uint64_t revision(){return 1;}static size_t cost(const ActorArt&){return 1;}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xffffffff);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,external::voxel::Image&i){i={};i.width=60;i.height=30;i.x=-30;i.y=-15;i.pixels.assign(1800,0xff202020);i.writeDepth=false;return true;}
 bool overlay(uint32_t,uint32_t,int,external::voxel::Image&,bool=false){return false;}
 bool shroud(bool,int,external::voxel::Image&){return false;}
 void buildingPose(uint32_t,uint32_t,external::visual::Pose&){}
 bool captureActor(uint32_t,uint32_t t,uint32_t,ActorArt&a){a.type=t;return true;}
 bool captureActor(uint32_t,uint32_t t,uint32_t,ActorArt&a,const external::visual::Pose*p){a.type=t;a.role=p->variant;return true;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool renderActor(const ActorArt&a,const external::visual::Pose&p,external::voxel::Image&i){
  if(a.type==Yard){++yardCalls;frames.insert(p.frame);if(unavailable)return false;assert(a.role==p.variant);}
  i={};i.width=i.height=2;i.writeDepth=false;i.pixels.assign(4,a.type==Yard?(0xff008000u|uint32_t(p.frame)):0xff004000);return true;
 }
};
int main(){using namespace external::radar;Assets assets;SceneRenderer<Assets>renderer;auto read=[](uint32_t,void*,size_t){return false;};
 auto t=std::make_shared<Terrain>();t->geo={8,8,6,7,8,8,1,0x10000,0x20000,512*512,{8,8,0,0,8,8},8,8};t->reveal=true;t->colors.resize(512*512);t->art.resize(512*512);t->shade.resize(512*512);t->colors[7*512+7]=0x202020;t->art[7*512+7].type=1;
 Frame f;f.terrain=t;f.frame=10;f.tick=100;f.posesPrepared=true;
 for(unsigned n=0;n<96;++n){Marker m;m.x=6*256;m.y=7*256;m.object=0x30000+n;m.id=n+1;m.type=0x40000+n;m.owner=0x20000;m.kind=2;f.markers.push_back(m);}
 Marker yard;yard.x=yard.y=7*256;yard.object=0x33300;yard.id=100;yard.type=Assets::Yard;yard.owner=0x20000;yard.kind=3;yard.pose.variant=-2;f.markers.push_back(yard);
 auto draw=[&](){auto image=renderer.render(f,240,120,read,assets,{0,0,240,120,true},0,true);assert(image);return image;};
 // Current native stage in both directions, despite continuous cache misses
 // for 96 unrelated movers placed ahead of the yard in the decode order.
 for(int direction:{1,-1})for(int n=0;n<24;++n){int stage=direction>0?n:23-n;f.markers.back().pose.frame=stage;
  for(size_t j=0;j+1<f.markers.size();++j)f.markers[j].pose.frame=n+(direction>0?50:100);
  f.tick+=20;auto image=draw();assert(assets.frames.count(stage));assert(image->pixels[90*240+180]==(0xff008000u|uint32_t(stage)));}
 // Same source pointer/type switches between make and normal art. A live
 // provider may already have advanced; capture must receive the sampled role.
 f.markers.back().pose.variant=-1;f.markers.back().pose.frame=0;f.tick+=20;draw();
 f.markers.back().pose.variant=-2;f.markers.back().pose.frame=24;f.tick+=20;assets.unavailable=true;draw();unsigned attempts=assets.yardCalls;
 assets.unavailable=false;f.tick+=17;auto recovered=draw();assert(assets.yardCalls>attempts&&recovered->pixels[90*240+180]==0xff008018u);
 f.markers.pop_back();f.tick+=20;auto removed=draw();assert(removed->pixels[90*240+180]!=0xff008018u);
 std::puts("PASS actual SceneRenderer: make/reverse stages remain visible ahead of 96 movers, sampled art-role dispatch, 17ms transient-read recovery and removed-building cleanup; synthetic art, no real-game visual claim");}
