#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <cstdio>
struct Assets {
 struct ActorArt {bool voxel=false;uint32_t type=0;struct Model{std::vector<uint8_t> faces;}model;};
 uint32_t vplColor=0xff80c040;bool failVXL=false,heavyModel=false;unsigned captures=0,voxels=0,shapes=0;
 uint64_t revision(){return 1;}static size_t cost(const ActorArt&){return sizeof(ActorArt);}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xff202020);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,external::voxel::Image&i){i={};i.width=60;i.height=30;i.x=-30;i.y=-15;i.pixels.assign(1800,0xff202020);i.writeDepth=false;return true;}
 bool overlay(uint32_t,uint32_t,int,external::voxel::Image&,bool=false){return false;}
 bool shroud(bool,int,external::voxel::Image&){return false;}
 void buildingPose(uint32_t,uint32_t,external::visual::Pose&){}
 bool captureActor(uint32_t,uint32_t type,uint32_t,ActorArt&art){++captures;art.voxel=type>=1000;art.type=type;if(art.voxel&&heavyModel)art.model.faces.resize(external::rendermemory::LiveFaces+1);return true;}
 bool captureAnimation(uint32_t p,uint32_t t,uint32_t id,uint32_t,uint32_t,ActorArt&a){return captureActor(p,t,id,a);}
 bool renderActor(const ActorArt&art,const external::visual::Pose&pose,external::voxel::Image&i){
  if(art.voxel){++voxels;if(failVXL)return false;}else ++shapes;
  i={};i.width=i.height=2;i.pixels.assign(4,art.voxel?(vplColor^uint32_t(pose.frame*0x110000)^uint32_t(std::max(0,pose.bodyFine))):0xff606080);i.depth.assign(4,float(art.voxel?10:1));i.writeDepth=false;return true;
 }
};
int main(){
 using namespace external;using namespace radar;Assets assets;SceneRenderer<Assets> renderer;auto read=[](uint32_t,void*,size_t){return false;};
 auto terrain=std::make_shared<Terrain>();terrain->geo={8,8,6,7,8,8,1,0x10000,0x20000,512*512,{8,8,0,0,8,8},8,8};terrain->reveal=true;terrain->colors.resize(512*512);terrain->art.resize(512*512);terrain->shade.resize(512*512);terrain->colors[7*512+7]=0x202020;terrain->art[7*512+7].type=1;
 Frame frame;frame.terrain=terrain;frame.frame=77;frame.tick=100;Marker shape;shape.x=shape.y=7*256;shape.object=0x10000;shape.id=1;shape.type=1;shape.owner=0x20000;shape.kind=2;frame.markers.push_back(shape);
 // More than eight visible models: the source-decode budget may delay loading,
 // but it may not freeze already-captured VXL RGB poses on later presentations.
 for(int n=0;n<12;++n){auto m=shape;m.object+=100+n;m.id+=100+n;m.type=1000+n;m.pose.voxel=true;m.pose.bodyFine=n;frame.markers.push_back(m);}
 auto draw=[&](){return renderer.render(frame,240,120,read,assets,{0,0,240,120,true},0,true);};
 std::shared_ptr<const Scene> first;for(int n=0;n<5&&!first;++n){++frame.tick;first=draw();}assert(first&&renderer.materialsReady());
 auto captures=assets.captures,voxels=assets.voxels,shapes=assets.shapes;assert(captures==13);auto same=draw();assert(same&&same->pixels==first->pixels);assert(assets.captures==captures&&assets.voxels==voxels+12&&assets.shapes==shapes);
 assets.vplColor=0xff40c080;voxels=assets.voxels;auto relit=draw();assert(relit&&relit->pixels!=same->pixels&&assets.voxels==voxels+12&&assets.captures==captures&&assets.shapes==shapes);
 // VXL source frame metadata is never used for two-image SHP crossfading.
 frame.markers.erase(frame.markers.begin()+2,frame.markers.end());frame.markers[1].pose.frame=1;frame.markers[1].pose.frameFrom=0;frame.markers[1].pose.frameWeight=0;
 auto current=draw();assert(current&&current->pixels[105*240+180]==(assets.vplColor^0x110000));
 frame.markers[1].pose.bodyFine=91;auto turned=draw();assert(turned&&turned->pixels[105*240+180]==(assets.vplColor^0x110000^91));assert(assets.captures==captures);
 assets.failVXL=true;assert(!draw());assets.failVXL=false;auto recovered=draw();assert(recovered&&recovered->pixels==turned->pixels&&assets.captures==captures);
 Assets heavy;heavy.heavyModel=true;SceneRenderer<Assets> bounded;frame.markers={frame.markers[1]};
 assert(!bounded.render(frame,240,120,read,heavy,{0,0,240,120,true},0,true));assert(heavy.captures==1&&heavy.voxels==0); // fail before an oversized source reaches the raster loop
 assert(frame.frame==77&&assets.revision()==1);
 std::puts("PASS: VXL current source raster every presentation, 12-model pass beyond source budget8, live VPL color and angle, no VXL image blend, raw geometry retention and failure fallback");
}
