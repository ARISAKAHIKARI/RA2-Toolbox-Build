#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <cstdio>
struct Assets {
 struct ActorArt{};unsigned captures=0,decodes=0;bool unavailable=false;
 uint64_t revision(){return 1;}static size_t cost(const ActorArt&){return 1;}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xffffffff);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,external::voxel::Image&i){i={};i.width=60;i.height=30;i.x=-30;i.y=-15;i.pixels.assign(1800,0xff202020);i.writeDepth=false;return true;}
 bool overlay(uint32_t,uint32_t,int,external::voxel::Image&,bool=false){return false;}
 bool shroud(bool,int frame,external::voxel::Image&i){i={};i.canvasWidth=i.width=60;i.canvasHeight=i.height=30;i.x=-30;i.y=-15;i.indices.assign(1800,frame==15?0:127);return true;}
 void buildingPose(uint32_t,uint32_t,external::visual::Pose&){}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&){++captures;return true;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool renderActor(const ActorArt&,const external::visual::Pose&p,external::voxel::Image&i){++decodes;if(unavailable)return false;i={};i.width=i.height=2;i.pixels.assign(4,p.frame?0xffff0000:0xff0000ff);i.writeDepth=false;return true;}
};
int main(){
 using namespace external::radar;Assets assets;SceneRenderer<Assets> renderer;auto read=[](uint32_t,void*,size_t){return false;};
 auto t=std::make_shared<Terrain>();t->geo={8,8,6,7,8,8,1,0x10000,0x20000,512*512,{8,8,0,0,8,8},8,8};t->reveal=true;t->colors.resize(512*512);t->art.resize(512*512);t->shade.resize(512*512);t->colors[7*512+7]=0x202020;t->art[7*512+7].type=1;
 Frame frame;frame.terrain=t;frame.frame=10;frame.tick=100;frame.posesPrepared=true;
 Marker m;m.x=m.y=7*256;m.object=0x30000;m.id=1;m.type=0x40000;m.owner=0x20000;m.kind=2;m.layer=3;m.pose.frame=1;m.pose.frameFrom=0;m.pose.frameWeight=128;frame.markers={m};
 auto middle=renderer.render(frame,240,120,read,assets,{0,0,240,120,true},3,true);assert(middle);
 assert(middle->pixels[105*240+180]==0xff80007f);assert(assets.captures==1&&assets.decodes==2&&renderer.materialsReady());
 auto original=middle->pixels;auto again=renderer.render(frame,240,120,read,assets,{0,0,240,120,true},3,true);assert(again&&again->pixels==original&&assets.decodes==2);
 frame.markers[0].pose.frameWeight=255;
 auto end=renderer.render(frame,240,120,read,assets,{0,0,240,120,true},3,true);assert(end&&end->pixels[105*240+180]==0xffff0000);
 auto large=renderer.render(frame,480,240,read,assets,{0,0,240,120,true},3,true);assert(large&&large->pixels[211*480+361]==0xffff0000);
 frame.markers.clear();auto removed=renderer.render(frame,240,120,read,assets,{0,0,240,120,true},3,true);assert(removed&&removed->pixels[105*240+180]==0xff202020);
 frame.markers={m};auto covered=std::make_shared<Terrain>(*t);covered->reveal=false;covered->shade[7*512+7]={-2,-1};frame.terrain=covered;
 auto fog=renderer.render(frame,480,240,read,assets,{0,0,240,120,true},3,true);assert(fog&&fog->pixels[211*480+361]==0xff000000);
 // Turning detail off restores the established native route and cached art.
 frame.terrain=t;frame.markers[0].pose.frameFrom=-1;auto native=renderer.render(frame,240,120,read,assets,{0,0,240,120,true},0,false);assert(native&&native->pixels[105*240+180]==0xffff0000);
 frame.markers[0].pose.frame=2;assets.unavailable=true;auto waiting=renderer.render(frame,240,120,read,assets,{0,0,240,120,true},0,true);assert(waiting&&!renderer.materialsReady());
 assets.unavailable=false;frame.tick+=2001;auto recovered=renderer.render(frame,240,120,read,assets,{0,0,240,120,true},0,true);assert(recovered&&renderer.materialsReady());
 std::puts("PASS: production SceneRenderer composes both observed SHP frames, target pixels, immutable source cache, actor removal, final shroud mask and native-mode recovery");
}
