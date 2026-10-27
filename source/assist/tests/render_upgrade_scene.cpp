#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <cstdio>
struct Assets {
 struct ActorArt{};int loads=0;
 uint64_t revision(){return 1;}static size_t cost(const ActorArt&){return 1;}
 bool palette(bool,std::array<uint32_t,256>&out){out.fill(0xffffffff);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,external::voxel::Image&i){
  ++loads;i.width=60;i.height=30;i.x=-30;i.y=-15;i.pixels.resize(1800);i.writeDepth=false;
  for(int y=0;y<30;++y)for(int x=0;x<60;++x)i.pixels[y*60+x]=0xff000000|uint32_t(x*4)<<16|uint32_t(y*8)<<8|0x33;
  return true;
 }
 bool overlay(uint32_t,uint32_t,int,external::voxel::Image&,bool=false){return false;}
 bool shroud(bool,int f,external::voxel::Image&i){i={};i.canvasWidth=i.width=60;i.canvasHeight=i.height=30;i.x=-30;i.y=-15;i.indices.resize(1800);for(int y=0;y<30;++y)for(int x=0;x<60;++x)i.indices[y*60+x]=f==15?2:uint8_t(x*127/59);return true;}
 void buildingPose(uint32_t,uint32_t,external::visual::Pose&){}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool renderActor(const ActorArt&,const external::visual::Pose&,external::voxel::Image&){return false;}
};
int main(){
 using namespace external::radar;auto read=[](uint32_t,void*,size_t){return false;};Assets assets;SceneRenderer<Assets> renderer;
 auto terrain=std::make_shared<Terrain>();terrain->geo={8,8,6,7,8,8,1,0x10000,0x20000,512*512,{8,8,0,0,8,8},8,8};
 terrain->colors.resize(512*512);terrain->art.resize(512*512);terrain->shade.resize(512*512);
 terrain->colors[7*512+7]=0x112233;terrain->art[7*512+7].type=1;terrain->shade[7*512+7]={0,-1};Frame frame;frame.terrain=terrain;frame.tick=100;
 auto whole=renderer.render(frame,240,120,read,assets);assert(whole);int loads=assets.loads;
 for(int y:{0,24,57})for(int x:{0,61,110}){
  auto crop=renderer.render(frame,80,60,read,assets,{double(x),double(y),80,60,true});assert(crop);
  for(int yy=0;yy<60;++yy)for(int xx=0;xx<80;++xx)assert(crop->pixels[yy*80+xx]==whole->pixels[(y+yy)*240+x+xx]);
  assert(assets.loads==loads); // camera movement did not rebuild sprite assets
 }
 auto edge=renderer.render(frame,40,40,read,assets,{-10,-10,40,40,true});assert(edge&&edge->pixels[0]==0);
 auto wide=renderer.render(frame,2560,60,read,assets,{0,0,2560,60,true});assert(wide&&wide->pixels[240]==0);
 auto old=renderer.render(frame,240,120,read,assets);assert(old&&old->pixels==whole->pixels);
 std::puts("PASS: SceneRenderer native crops equal whole-scene pixels including shroud; 2560px; cache reuse; default minimap restored");
}
