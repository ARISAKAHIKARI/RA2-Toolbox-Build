#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <cstdio>
struct ParityAssets {
 struct ActorArt {bool voxel=false;struct Model{std::vector<uint8_t>faces;}model;};
 unsigned captures=0,vxlRaster=0,shapeRaster=0;
 uint64_t vplRevision=1;uint32_t vplColor=0xffe08040;
 uint64_t revision(){return 1;}
 uint64_t voxelLightingRevision()const{return vplRevision;}
 static size_t cost(const ActorArt&){return sizeof(ActorArt);}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xff404040);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,external::voxel::Image&i){i={};i.width=60;i.height=30;i.x=-30;i.y=-15;i.pixels.assign(1800,0xff404040);i.writeDepth=false;return true;}
 bool overlay(uint32_t,uint32_t,int,external::voxel::Image&,bool=false){return false;}
 bool shroud(bool,int,external::voxel::Image&){return false;}
 void buildingPose(uint32_t,uint32_t,external::visual::Pose&){}
 bool captureActor(uint32_t,uint32_t t,uint32_t,ActorArt&a){++captures;a.voxel=t>=1000;return true;}
 bool captureAnimation(uint32_t p,uint32_t t,uint32_t id,uint32_t,uint32_t,ActorArt&a){return captureActor(p,t,id,a);}
 bool renderActor(const ActorArt&a,const external::visual::Pose&pose,external::voxel::Image&i){
  if(a.voxel)++vxlRaster;else ++shapeRaster;
  i={};i.width=i.height=2;i.writeDepth=false;
  i.pixels.assign(4,a.voxel?(vplColor^uint32_t(pose.bodyFine)):0xff80b080u);
  return true;
 }
};
int main(){
 using namespace external;using namespace external::radar;
 auto read=[](uint32_t,void*,size_t){return false;};
 auto terrain=std::make_shared<Terrain>();terrain->geo={8,8,6,7,8,8,1,0x10000,0x20000,512*512,{8,8,0,0,8,8},8,8};
 terrain->reveal=true;terrain->colors.resize(512*512);terrain->art.resize(512*512);terrain->shade.resize(512*512);
 for(int y=0;y<8;++y)for(int x=0;x<8;++x){terrain->colors[y*512+x]=0x404040;terrain->art[y*512+x].type=1;}
 Frame frame;frame.terrain=terrain;frame.frame=40;frame.tick=100;
 for(int n=0;n<6;++n){Marker m;m.x=(3+n%3)*256;m.y=(4+n/3)*256;m.object=0x10000u+n;m.id=n+1;m.owner=0x20000;m.kind=n%2?2:3;m.type=n%2?1000u+n:5u;
  m.pose.voxel=m.type>=1000;m.pose.bodyFine=n;frame.markers.push_back(m);}
 ParityAssets normal,detail;SceneRenderer<ParityAssets> basicRenderer,detailedRenderer;
 auto render=[&](SceneRenderer<ParityAssets>&r,ParityAssets&a,bool fine){std::shared_ptr<const Scene>scene;
  for(int n=0;n<12&&!scene;++n){frame.tick++;scene=r.render(frame,240,120,read,a,{0,0,240,120,true},1,fine,true,false,true);}
  assert(scene);return scene;
 };
 auto basic=render(basicRenderer,normal,false),fine=render(detailedRenderer,detail,true);
 auto bc=basicRenderer.objectCoverage(),dc=detailedRenderer.objectCoverage();
 assert(bc.second==6&&dc.second==6&&bc.first==6&&dc.first==6);
 const unsigned cold=normal.vxlRaster;auto again=render(basicRenderer,normal,false);
 assert(again->pixels==basic->pixels&&normal.vxlRaster==cold&&normal.captures==4);
 normal.vplColor=0xff30b0e0;normal.vplRevision++;
 auto changed=render(basicRenderer,normal,false);
 assert(changed->pixels!=basic->pixels&&normal.vxlRaster==cold+3);
 assert(basicRenderer.objectCoverage().first==6);
 std::printf("PASS: basic/detailed marker coverage %u/%u; stable VPL CPU VXL raster %u->%u, VPL revision re-raster %u\n",
  bc.first,bc.second,cold,cold,normal.vxlRaster);
}
