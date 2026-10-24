#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <iostream>
struct Assets {
 struct ActorArt {int id=0;};struct Files {uint64_t value=1;uint64_t revision(){return value;}}files;int loads=0;
 static size_t cost(const ActorArt&){return sizeof(ActorArt);}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xffffffff);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,external::voxel::Image&i){++loads;i.width=60;i.height=30;i.x=-30;i.y=-15;i.pixels.assign(1800,0xffa03020);i.depth.assign(1800,0);return true;}
 bool overlay(uint32_t,uint32_t,int,external::voxel::Image&,bool=false){return false;}
 int buildingFrame(uint32_t,uint32_t){return 0;}
 bool captureActor(uint32_t,uint32_t,uint32_t id,ActorArt&a){a.id=id;return true;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool renderActor(const ActorArt&,const external::visual::Pose&,external::voxel::Image&i){i.width=i.height=60;i.x=i.y=-30;i.pixels.assign(3600,0xff00aa00);i.depth.assign(3600,5);i.writeDepth=false;return true;}
};
int main(){
 using namespace external::radar;Assets assets;SceneRenderer<Assets> renderer;auto read=[](uint32_t,void*,size_t){return false;};
 auto terrain=std::make_shared<Terrain>();terrain->geo={64,64,64,64,128,128,1,0x10000,0x20000,512*512};terrain->colors.resize(512*512);terrain->art.resize(512*512);
 terrain->colors[64*512+64]=0xa03020;terrain->art[64*512+64].type=1;Frame frame;frame.terrain=terrain;frame.tick=100;
 assert(!renderer.render(frame,128,64,read,assets));std::shared_ptr<const Scene> scene;
 for(int i=0;i<80&&!scene;++i)scene=renderer.render(frame,128,64,read,assets);
 assert(scene&&scene->width==128&&scene->height==64&&assets.loads==1);int loads=assets.loads;
 Marker m;m.u=m.v=.5;m.x=m.y=64*256+128;m.object=0x10000;m.type=0x20000;m.id=1;m.kind=2;frame.markers={m};
 scene=renderer.render(frame,128,64,read,assets);assert(scene&&std::find(scene->pixels.begin(),scene->pixels.end(),0xff00aa00)!=scene->pixels.end()&&assets.loads==loads);
 auto old=scene;auto replacement=std::make_shared<Terrain>(*terrain);frame.terrain=replacement;scene=renderer.render(frame,128,64,read,assets);assert(scene&&old->pixels.size()==128*64); // completed background stays visible while refreshing
 assert(!renderer.render(frame,64,32,read,assets));for(int i=0;i<80&&!scene;++i)scene=renderer.render(frame,64,32,read,assets);
 scene.reset();for(int i=0;i<80&&!scene;++i)scene=renderer.render(frame,64,32,read,assets);assert(scene&&scene->width==64&&scene->height==32);
 ++assets.files.value;assert(!renderer.render(frame,64,32,read,assets));assert(!renderer.render(frame,100000,2,read,assets));
 std::cout<<"PASS scene terrain completion, cached real sprites, paused-frame progress, immutable frames, resize and resource generation\n";
}
