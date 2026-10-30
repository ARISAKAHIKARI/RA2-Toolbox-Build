#include "../source/render_presentation.h"
#include "../../external_reader/source/feedback_scene.h"
#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <cstdio>
#include <limits>
struct Request {
 uint64_t generation=1;bool materialRefresh=true;int detail=1,colorPrecision=1;uint32_t pid=77,match=8,tick=1000;
 int width=640,height=400;uint32_t reveal=0;int paletteRange=1,pixelScaleMode=3;
 external::mainview::View view{0x10000,0x20000,0x30000,50,300,400,640,480,0,0,320,200};
 overlaycomposition::Context context{9,101,77,8,700,500,300,400};
};
struct Image {int width=0,height=0;std::vector<uint32_t>pixels;};
struct Frame {Request request;std::shared_ptr<const Image>image;uint32_t sampledTick=1000,completedTick=1200;int paddingX=0,paddingY=0;};
uint32_t color(int x,int y){return 0xff000000u|(uint32_t(x)&255)<<16|(uint32_t(y)&255)<<8|((uint32_t(x)^uint32_t(y))&255);}
struct Marker {uint32_t object=0x60000,id=1,type=0x70000,owner=0x20000;int kind=0,x=1024,y=1024,z=0;};
struct GeometryFrame {std::vector<Marker>markers;};
struct GeometryRenderer {bool feedbackBounds(const Marker&,int&x,int&y,int&w,int&h)const{x=-10;y=-10;w=20;h=20;return true;}};
struct SceneAssets {
 struct ActorArt{};uint64_t revision(){return 1;}static size_t cost(const ActorArt&){return 1;}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xffffffff);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,external::voxel::Image&i){
  i.width=60;i.height=30;i.x=-30;i.y=-15;i.writeDepth=false;i.pixels.resize(1800);
  for(int y=0;y<30;++y)for(int x=0;x<60;++x)i.pixels[y*60+x]=color(x,y);return true;
 }
 bool overlay(uint32_t,uint32_t,int,external::voxel::Image&,bool=false){return false;}
 bool shroud(bool,int,external::voxel::Image&){return false;}
 void buildingPose(uint32_t,uint32_t,external::visual::Pose&){}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool renderActor(const ActorArt&,const external::visual::Pose&,external::voxel::Image&){return false;}
};
int main(){
 using namespace renderpresentation;
 StatusCadence status;unsigned statusUpdates=0;
 for(uint32_t tick=0;tick<1000;tick+=2)statusUpdates+=status.due(2,tick);
 assert(statusUpdates==4);
 Request sampled;auto im=std::make_shared<Image>();im->width=sampled.width;im->height=sampled.height;
 im->pixels.assign(size_t(im->width)*im->height,0xff123456);Frame frame{sampled,im,1000,1200,0,0};
 for(int dy:{-600,0,600})for(int dx:{-900,0,900}){
  auto current=sampled;current.view.cameraX+=dx;current.view.cameraY+=dy;
  assert(completeUsable(frame,current,1250));assert(usable(frame,current,1250)==(!dx&&!dy));
  overlaycomposition::Surface layer{3,5,sampled.context,{13,7,im->width,im->height},true,255,std::shared_ptr<const std::vector<uint32_t>>(im,&im->pixels),{13,7,im->width,im->height}};
  std::vector<uint32_t>out;assert(overlaycomposition::compose(sampled.context,{layer},out));
  assert(out.front()==0&&out.back()==0&&out[7*700+13]==0xff123456);
 }
 auto invalid=sampled;invalid.view.scenario++;assert(!completeUsable(frame,invalid,1250));
 invalid=sampled;invalid.generation++;assert(!completeUsable(frame,invalid,1250));
 assert(!completeUsable(frame,sampled,1300));
 im->width++;assert(!completeUsable(frame,sampled,1250));im->width--;
 auto terrain=std::make_shared<external::radar::Terrain>();
 terrain->geo={8,8,6,7,8,8,1,0x10000,0x20000,512*512,{8,8,0,0,8,8},8,8};terrain->reveal=true;
 terrain->colors.resize(512*512);terrain->art.resize(512*512);terrain->shade.resize(512*512);
 for(int y=1;y<512;++y)for(int x=1;x<512;++x)if(terrain->geo.legal(x,y)){size_t n=size_t(y)*512+x;terrain->colors[n]=0x112233;terrain->art[n].type=1;}
 external::radar::Frame world;world.terrain=terrain;world.tick=100;
 external::radar::SceneRenderer<SceneAssets> prepared,oracle;SceneAssets assets;prepared.prepareWorld(true);
 auto read=[](uint32_t,void*,size_t){return false;};
 auto packet=prepared.render(world,160,120,read,assets,{80,60,80,60,true},1,true,true,true,true);
 assert(packet&&packet->pixels.empty()&&prepared.preparedWorld());
 const auto snapshot=prepared.preparedWorld();const auto pages=prepared.nativeStats().pages;const auto painted=prepared.nativeStats().cells;
 assert(pages>0);external::worldscene::Pipeline pipeline;size_t checked=0;
 for(int dy:{-100,0,100})for(int dx:{-200,0,200}){
  auto camera=sampled.view;camera.width=80;camera.height=60;
  camera.cameraX=80+dx-terrain->geo.offsetX*30;camera.cameraY=60+dy+terrain->geo.offsetY*15;
  auto result=pipeline.draw(snapshot,camera,160,120,read);assert(result);
  auto expected=oracle.render(world,160,120,read,assets,{80.+dx,60.+dy,80,60,true},1,true,true,true,true);assert(expected);
  assert(result->pixels==expected->pixels);checked+=result->pixels.size();
  assert(prepared.render(world,160,120,read,assets,{80.+dx,60.+dy,80,60,true},1,true,true,true,true));
  assert(prepared.nativeStats().pages==pages&&prepared.nativeStats().cells==painted);
 }
 std::printf("PASS: %zu retained-world/current-camera pixels; whole-frame publication, sidebar scissor and no camera-driven terrain rebuild; CPU only\n",checked);
}
