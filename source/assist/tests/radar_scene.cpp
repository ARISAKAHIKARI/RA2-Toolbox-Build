#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <iostream>
struct Assets {
 struct ActorArt {int id=0;bool anim=false;};struct Files {uint64_t value=1;uint64_t revision(){return value;}}files;int loads=0;std::set<int> seen;bool small=false;
 uint64_t revision(){return files.revision();}
 static size_t cost(const ActorArt&){return sizeof(ActorArt);}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xffffffff);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,external::voxel::Image&i){++loads;i.width=60;i.height=30;i.x=-30;i.y=-15;i.pixels.assign(1800,0xffa03020);i.depth.assign(1800,0);return true;}
 bool overlay(uint32_t,uint32_t,int,external::voxel::Image&,bool=false){return false;}
 int buildingFrame(uint32_t,uint32_t){return 0;}
 bool captureActor(uint32_t,uint32_t,uint32_t id,ActorArt&a){a.id=id;return true;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t id,ActorArt&a){a.id=id;a.anim=true;return true;}
 bool renderActor(const ActorArt&a,const external::visual::Pose&,external::voxel::Image&i){seen.insert(a.id);i.width=i.height=small?8:60;i.x=i.y=-i.width/2;i.pixels.assign(i.width*i.height,a.anim?0xffaaff00:0xff00aa00);i.depth.assign(i.width*i.height,a.anim?100:5);i.writeDepth=false;return true;}
};
int main(){
 using namespace external::radar;Assets assets;SceneRenderer<Assets> renderer;auto read=[](uint32_t,void*,size_t){return false;};
 auto terrain=std::make_shared<Terrain>();terrain->geo={64,64,64,64,128,128,1,0x10000,0x20000,512*512};terrain->colors.resize(512*512);terrain->art.resize(512*512);
 terrain->colors[64*512+64]=0xa03020;terrain->art[64*512+64].type=1;Frame frame;frame.terrain=terrain;frame.tick=100;
 std::shared_ptr<const Scene> scene=renderer.render(frame,128,64,read,assets);
 for(int i=0;i<80&&!scene;++i)scene=renderer.render(frame,128,64,read,assets);
 assert(scene&&scene->width==128&&scene->height==64&&assets.loads==1);int loads=assets.loads;
 Marker m;m.u=m.v=.5;m.x=m.y=64*256+128;m.object=0x10000;m.type=0x20000;m.id=1;m.kind=2;frame.markers={m};
 scene=renderer.render(frame,128,64,read,assets);assert(scene&&std::find(scene->pixels.begin(),scene->pixels.end(),0xff00aa00)!=scene->pixels.end()&&assets.loads==loads);
 auto old=scene;auto replacement=std::make_shared<Terrain>(*terrain);frame.terrain=replacement;scene=renderer.render(frame,128,64,read,assets);assert(scene&&old->pixels.size()==128*64); // completed background stays visible while refreshing
 scene=renderer.render(frame,64,32,read,assets);assert(!scene||scene->width==64);
 scene.reset();for(int i=0;i<80&&!scene;++i)scene=renderer.render(frame,64,32,read,assets);assert(scene&&scene->width==64&&scene->height==32);
 ++assets.files.value;int oldLoads=assets.loads;renderer.render(frame,64,32,read,assets);assert(assets.loads>oldLoads);assert(!renderer.render(frame,100000,2,read,assets));
 // Continuous motion formerly exhausted the first eight decode slots forever.
 renderer.reset();assets.small=true;assets.seen.clear();frame.markers.clear();
 for(int n=0;n<32;++n){auto moving=m;moving.id=n+1;moving.object=0x10000+n*0x800;moving.type=0x50000+n*0x800;moving.u=.05+n*.025;moving.v=.25;frame.markers.push_back(moving);}
 auto pump=m;pump.kind=5;pump.id=99;pump.object=0x80000;pump.type=0x90000;pump.u=pump.v=.75;pump.pose.frame=0;frame.markers.push_back(pump);
 for(int tick=0;tick<12;++tick){for(auto&marker:frame.markers)if(marker.kind!=5)marker.pose.frame=tick;frame.tick+=16;scene=renderer.render(frame,1024,512,read,assets);}
 assert(assets.seen.size()==33&&assets.seen.count(99));
 assert(std::find(scene->pixels.begin(),scene->pixels.end(),0xffaaff00)!=scene->pixels.end());
 for(int tick=0;tick<4;++tick){for(auto&marker:frame.markers)++marker.pose.frame;frame.tick+=16;scene=renderer.render(frame,1024,512,read,assets);assert(std::find(scene->pixels.begin(),scene->pixels.end(),0xffaaff00)!=scene->pixels.end());} // last good anim stays visible under budget
 // A body later in XY order must not paint over its attached animation.
 renderer.reset();assets.small=false;frame.markers={m,pump};frame.markers[0].u=frame.markers[0].v=.5;frame.markers[0].kind=3;frame.markers[1].u=frame.markers[1].v=.5;frame.markers[1].x=m.x-1;frame.markers[1].y=m.y-1;
 scene=renderer.render(frame,128,64,read,assets);assert(scene->pixels[32*128+64]==0xffaaff00);
 // Removing/hiding an attachment must remove its retained pose immediately.
 frame.markers.resize(1);scene=renderer.render(frame,128,64,read,assets);assert(std::find(scene->pixels.begin(),scene->pixels.end(),0xffaaff00)==scene->pixels.end());
 std::cout<<"PASS complete scene publication, first-frame building anims, moving-object budget fairness, retained poses, deferred animation pass, removal, resize and theater/resource reset\n";
}
