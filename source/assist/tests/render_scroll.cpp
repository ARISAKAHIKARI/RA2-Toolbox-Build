#include "../source/render_presentation.h"
#include "../../external_reader/source/feedback_scene.h"
#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <cstdio>
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
 using namespace renderpresentation;Request sampled;auto border=window(sampled.width,sampled.height,sampled.view.width,sampled.view.height);
 StatusCadence status;unsigned statusUpdates=0;
 for(uint32_t tick=0;tick<1000;tick+=2)statusUpdates+=status.due(2,tick);
 assert(statusUpdates==4&&status.due(1,999)&&!status.due(1,1000)&&status.due(10,1001));
 StatusCadence wrapping;assert(wrapping.due(2,0xfffffff0u)&&!wrapping.due(2,100)&&wrapping.due(2,300));
 assert(ScrollBorder==96&&border.paddingX==192&&border.paddingY==192&&border.width==1024&&border.height==784);
 auto im=std::make_shared<Image>();im->width=border.width;im->height=border.height;im->pixels.resize(size_t(im->width)*im->height);
 for(int y=0;y<im->height;++y)for(int x=0;x<im->width;++x)im->pixels[size_t(y)*im->width+x]=color(sampled.view.cameraX*2+x-border.paddingX,sampled.view.cameraY*2+y-border.paddingY);
 Frame frame{sampled,im,1000,1200,border.paddingX,border.paddingY};
 auto pixels=std::shared_ptr<const std::vector<uint32_t>>(im,&im->pixels);
 size_t checked=0;
 for(int dy:{-96,-60,-7,0,13,59,96})for(int dx:{-96,-50,-1,0,17,72,96}){
  Request current=sampled;current.view.cameraX+=dx;current.view.cameraY+=dy;current.context.cameraX+=dx;current.context.cameraY+=dy;current.view.frame++;
  assert(sameProjection(sampled.view,current.view));assert(usable(frame,current,1250)==(!dx&&!dy));
  Shift shift;assert(scrollUsable(frame,current,1250,shift));
  overlaycomposition::Surface scene{3,5,current.context,{13+shift.x,7+shift.y,im->width,im->height},true,255,pixels,{13,7,640,400}};
  assert(scene.pixels==pixels); // shared immutable overscan, no viewport copy
  std::vector<uint32_t>out;assert(overlaycomposition::compose(current.context,{scene},out));
  for(int y=0;y<500;++y)for(int x=0;x<700;++x){
   const uint32_t expected=x>=13&&x<653&&y>=7&&y<407?color(current.view.cameraX*2+x-13,current.view.cameraY*2+y-7):0;
   assert(out[size_t(y)*700+x]==expected);++checked;
  }
  auto spans=std::make_shared<const std::vector<overlaycomposition::Rect>>(std::vector<overlaycomposition::Rect>{{10,20,8,1}});
  scene.cutouts=sceneCutouts(spans,shift);assert(overlaycomposition::compose(current.context,{scene},out));
  for(int x=23;x<31;++x)assert(out[size_t(27)*700+x]==0);
 }
 Request changed=sampled;Shift shift;
 changed.view.cameraX+=97;assert(!scrollUsable(frame,changed,1250,shift));
 changed=sampled;changed.view.scenario++;assert(!scrollUsable(frame,changed,1250,shift));
 changed=sampled;changed.view.house++;assert(!scrollUsable(frame,changed,1250,shift));
 changed=sampled;changed.generation++;assert(!scrollUsable(frame,changed,1250,shift));
 changed=sampled;changed.paletteRange=0;assert(!scrollUsable(frame,changed,1250,shift));
 changed=sampled;changed.view.frame--;assert(!scrollUsable(frame,changed,1250,shift));
 assert(!scrollUsable(frame,sampled,2700,shift));
 assert(!window(9000,100,9000,100).width&&!window(8192,8192,8192,8192).width);
 auto limited=window(8192,2048,8192,2048);assert(limited.width==8192&&limited.height==2048&&limited.paddingX==0&&limited.paddingY==0);
 auto fourK=window(3840,2160,1920,1080);assert(fourK.width==4224&&fourK.height==2544); // ~29.6% more pixels, no sample reduction
 // Fractional target ratios resolve a camera translation to the closest
 // destination pixel. The quantization error is bounded by half a pixel.
 Request fractional=sampled;fractional.width=480;fractional.height=300;
 auto fb=window(480,300,320,200);Frame ff=frame;ff.request=fractional;ff.paddingX=fb.paddingX;ff.paddingY=fb.paddingY;
 auto fi=std::make_shared<Image>();fi->width=fb.width;fi->height=fb.height;ff.image=fi;
 for(int dx=-95;dx<=95;++dx){auto current=fractional;current.view.cameraX+=dx;
  assert(scrollUsable(ff,current,1250,shift));assert(std::abs(double(-shift.x-fb.paddingX)-dx*1.5)<=.5);
 }
 // Run the production terrain renderer too: an overscan frame cropped after
 // a 2x camera translation equals a fresh render of that destination view.
 auto terrain=std::make_shared<external::radar::Terrain>();
 terrain->geo={8,8,6,7,8,8,1,0x10000,0x20000,512*512,{8,8,0,0,8,8},8,8};terrain->reveal=true;
 terrain->colors.resize(512*512);terrain->art.resize(512*512);terrain->shade.resize(512*512);
 terrain->colors[7*512+7]=0x112233;terrain->art[7*512+7].type=1;
 external::radar::Frame world;world.terrain=terrain;world.tick=100;
 external::radar::SceneRenderer<SceneAssets> renderer;SceneAssets assets;
 auto read=[](uint32_t,void*,size_t){return false;};
 auto surrounding=renderer.render(world,160+384,120+384,read,assets,{80-96.,60-96.,80+192.,60+192.,true},1);
 assert(surrounding);
 for(int dy:{-20,0,20})for(int dx:{-20,0,20}){
  auto destination=renderer.render(world,160,120,read,assets,{80.+dx,60.+dy,80,60,true},1);assert(destination);
  for(int y=0;y<120;++y)for(int x=0;x<160;++x)
   assert(destination->pixels[size_t(y)*160+x]==surrounding->pixels[size_t(192+dy*2+y)*surrounding->width+192+dx*2+x]);
 }
 // Bounds for a unit just outside the source viewport are needed once the
 // camera moves into the overscan. Their coordinates remain viewport based.
 GeometryFrame gf;gf.markers.emplace_back();auto view=sampled.view;view.cameraX=60;view.cameraY=0;
 assert(external::nativefeedback::objectGeometry(GeometryRenderer{},gf,view,640,400,.08660254).empty());
 auto objects=external::nativefeedback::objectGeometry(GeometryRenderer{},gf,view,640,400,.08660254,192,192);
 assert(objects.size()==1&&objects[0].bounds.x<0);
 // Electric arcs use the same viewport scale and explicit overscan offset,
 // rather than stretching the viewport to fill the larger source image.
 view.width=128;view.height=64;view.cameraX=0;view.cameraY=80;
 std::vector<external::nativefeedback::Bolt>bolts={{{768,768,0},{1536,768,0},42,false}};
 Image normal;normal.width=128;normal.height=64;normal.pixels.assign(128*64,0xff102030);
 Image expanded;expanded.width=160;expanded.height=96;expanded.pixels.assign(160*96,0xff102030);
 assert(external::nativefeedback::effects(bolts,normal,view,nullptr,.08660254).complete);
 assert(external::nativefeedback::effects(bolts,expanded,view,nullptr,.08660254,128,64,16,16).complete);
 for(int y=0;y<64;++y)for(int x=0;x<128;++x)assert(normal.pixels[size_t(y)*128+x]==expanded.pixels[size_t(y+16)*160+x+16]);
 std::printf("PASS: %zu exact scroll/clip pixels; shared source pixels, all four overscan limits, native cutout rebasing, sidebars, world/config rejection, size budgets and object/effect geometry\n",checked);
}
