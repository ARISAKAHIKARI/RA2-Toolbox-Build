#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace external;
struct Read {
 int ambient=100;
 bool operator()(uint32_t p,void*out,size_t n)const{
  const uint32_t scenario=0xa0000;const int tint[3]={100,100,100};
  if(p==0xa8b230&&n==4){std::memcpy(out,&scenario,n);return true;}
  if(p==scenario+0x352c&&n==4){std::memcpy(out,&ambient,n);return true;}
  if(p==scenario+0x3534&&n==12){std::memcpy(out,tint,n);return true;}
  return false;
 }
};
struct Assets {
 struct ActorArt{};
 uint32_t color=0xff304038;int loads=0,captures=0,actorMode=0;bool cliff=false;
 uint64_t revision(){return 1;}static size_t cost(const ActorArt&){return 1;}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(color);p[0]=0;return true;}
 voxel::Image tileImage(uint32_t type)const{
  voxel::Image i;
  if(type==2){i.width=512;i.height=8;i.x=-470;i.y=-25;i.pixels.assign(4096,0xff509060);i.depth.assign(4096,0);return i;}
  // A native diamond plus a tall, stepped extra image. Its subdued edge is
  // deliberately a case where filtering an already flattened RGB backing
  // produces different colors from reconstructing the source cutout first.
  i.width=80;i.height=45;i.x=-30;i.y=-30;i.pixels.assign(3600,0);i.indices.assign(3600,0);i.depth.assign(3600,0);
  auto put=[&](int x,int y,uint32_t p){size_t at=size_t(y)*i.width+x;i.pixels[at]=p;i.indices[at]=1;i.depth[at]=cliff&&x>=32?60:0;};
  for(int y=0;y<30;++y){int w=4*std::min(y+1,29-y),l=(60-w)/2;for(int x=l;x<l+w;++x)put(x,y+15,color);}
  for(int y=1;y<14;++y)for(int x=4+y/2;x<16;++x){uint32_t c=color+(unsigned(x-4)*0x00010101u);put(x,y,c);}
  return i;
 }
 bool tile(uint32_t type,unsigned,const std::array<uint32_t,256>&,voxel::Image&i){++loads;i=tileImage(type);return true;}
 voxel::Image overlayImage()const{
  voxel::Image i;i.width=i.height=4;i.y=-9;i.pixels.assign(16,0xff8090a0);i.depth.assign(16,25);
  voxel::Image shadow;shadow.width=shadow.height=4;shadow.x=8;shadow.y=-9;shadow.pixels.assign(16,0xffffffff);shadow.depth.assign(16,20);shadow.shadow=true;shadow.writeDepth=true;i.attachments.push_back(shadow);return i;
 }
 bool overlay(uint32_t,uint32_t,int,voxel::Image&i,bool=false){i=overlayImage();return true;}
 bool shroud(bool,int frame,voxel::Image&i){i={};i.canvasWidth=i.width=60;i.canvasHeight=i.height=30;i.x=-30;i.y=-15;i.indices.assign(1800,frame==15?0:127);return true;}
 void buildingPose(uint32_t,uint32_t,visual::Pose&){}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&){++captures;return true;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool renderActor(const ActorArt&,const visual::Pose&p,voxel::Image&i){
  i={};i.width=8;i.height=12;i.pixels.resize(96);i.depth.assign(96,p.frame?100:20);i.writeDepth=false;
  for(int y=0;y<12;++y)for(int x=0;x<8;++x)i.pixels[y*8+x]=actorMode?0xff8090a0u+unsigned(x)*0x00080808u:0xffff0000u;
  return true;
 }
};
static std::shared_ptr<radar::Terrain> makeTerrain(uint32_t type=1,bool overlay=false){
 auto t=std::make_shared<radar::Terrain>();t->geo={8,8,6,7,8,8,1,0x10000,0x20000,512*512,{8,8,0,0,8,8},8,8};t->reveal=true;
 t->colors.resize(512*512);t->art.resize(512*512);t->shade.resize(512*512);size_t at=7*512+7;t->colors[at]=0x182818;t->art[at].type=type;
 if(overlay){t->art[at].overlayType=0x50000;t->art[at].overlay=0;}return t;
}
static radar::Marker actor(int frame=0){radar::Marker m;m.x=m.y=7*256;m.object=0x30000;m.id=1;m.type=0x40000;m.owner=0x20000;m.kind=2;m.layer=3;m.pose.frame=frame;return m;}
static std::shared_ptr<const radar::Scene> render(radar::SceneRenderer<Assets>&renderer,Assets&assets,const radar::Frame&frame,const Read&read,int w,int h,bool refined,unsigned mode=3,radar::SceneRegion region={0,0,240,120,true}){
 auto result=renderer.render(frame,w,h,read,assets,region,mode,refined);assert(result);return result;
}
static std::vector<uint32_t> reference(const Assets&assets,const Read&read,double scale,bool overlay){
 const int w=int(240*scale),h=int(120*scale);camera::Surface target;assert(target.reset(w,h));
 for(int y=0;y<h;++y)for(int x=0;x<w;++x)if((x+.5)/scale>=180&&(y+.5)/scale>=105)target.color[size_t(y)*w+x]=0xff182818;
 const auto light=lighting::sample(read);target.blit(assets.tileImage(1),float(180*scale),float(120*scale),float(scale),float(scale),210,light,0,0,true);
 if(overlay)target.blit(assets.overlayImage(),float(180*scale),float(120*scale),float(scale),float(scale),210,light,2,0,true);
 return target.color;
}
int main(){
 Read read;Assets assets;radar::Frame frame;frame.terrain=makeTerrain(1,true);frame.tick=100;frame.frame=1;frame.posesPrepared=true;
 radar::SceneRenderer<Assets> renderer;
 auto native=render(renderer,assets,frame,read,240,120,false);auto detailed=render(renderer,assets,frame,read,240,120,true);
 assert(detailed->pixels==native->pixels); // 1:1 preserves established native composition exactly
 assert(native->pixels==reference(assets,read,1,true)); // independently verifies the TMP/extra anchor
 const int decoded=assets.loads;
 for(double scale:{1.5,2.,3.,4.}){
  int w=int(240*scale),h=int(120*scale);auto fine=render(renderer,assets,frame,read,w,h,true);auto coarse=render(renderer,assets,frame,read,w,h,false);
  assert(fine->pixels==reference(assets,read,scale,true));
  assert(fine->pixels!=coarse->pixels); // source cutout/extra is recomposed at target pixels
  auto again=render(renderer,assets,frame,read,w,h,true);assert(again->pixels==fine->pixels&&assets.loads==decoded);
  // The overlay's detached shadow hits a constant opaque part of the TMP.
  // It halves the ground once; it must not darken the cached native shadow again.
  uint32_t ground=camera::lit(assets.color,1,false,lighting::sample(read));uint32_t half=0xff000000;
  for(int shift:{0,8,16})half|=(((ground>>shift)&255)/2)<<shift;
  assert(fine->pixels[size_t(int(112*scale))*w+int(189*scale)]==half);
 }
 // Live palette and local scene lighting remain inputs to the target terrain
 // bank; a stable decoded image must never freeze either color source.
 frame.terrain=makeTerrain();assets.color=0xff709040;auto recolored=render(renderer,assets,frame,read,480,240,true);
 assert(recolored->pixels==reference(assets,read,2,false));int paletteLoads=assets.loads;
 read.ambient=50;auto dimmed=render(renderer,assets,frame,read,480,240,true);assert(dimmed->pixels==reference(assets,read,2,false));assert(dimmed->pixels!=recolored->pixels&&assets.loads==paletteLoads);
 read.ambient=100;
 // Nearest source Z is deliberate at a cliff: the 60-unit discontinuity must
 // remain a sharp occlusion boundary even at noninteger physical density.
 Assets depthAssets;depthAssets.cliff=true;radar::SceneRenderer<Assets> depthRenderer;frame.terrain=makeTerrain();frame.markers={actor()};
 for(double scale:{1.5,2.,3.,4.}){
  int w=int(240*scale),h=int(120*scale);auto blocked=render(depthRenderer,depthAssets,frame,read,w,h,true);
  assert(blocked->pixels[size_t(int(112*scale))*w+int(181*scale)]==0xffff0000u); // actor Z215 exceeds near terrain Z210
  assert(blocked->pixels[size_t(int(112*scale))*w+int(184*scale)]!=0xffff0000u); // foreground Z270 still covers it
 }
 frame.markers[0].pose.frame=1;auto above=render(depthRenderer,depthAssets,frame,read,720,360,true);assert(above->pixels[size_t(337)*720+553]==0xffff0000u);
 // A wide TMP extra whose origin is outside the crop still enters it. Culling
 // by the 60x30 diamond or by the cell center would incorrectly discard it.
 Assets wideAssets;radar::SceneRenderer<Assets> wideRenderer;frame.terrain=makeTerrain(2);frame.markers.clear();auto extra=render(wideRenderer,wideAssets,frame,read,72,24,true,3,{0,95,24,8,true});
 assert(std::all_of(extra->pixels.begin(),extra->pixels.end(),[](uint32_t p){return p==0xff509060;}));
 auto margin=render(wideRenderer,wideAssets,frame,read,72,24,true,3,{-2,95,24,8,true});
 for(int y=0;y<24;++y)for(int x=0;x<72;++x)assert(margin->pixels[size_t(y)*72+x]==(x<6?0:0xff509060u));
 // The explicit nearest setting also governs dynamic target-pixel SHPs.
 Assets actorAssets;actorAssets.actorMode=1;radar::SceneRenderer<Assets> actorRenderer;frame.terrain=makeTerrain();frame.markers={actor(1)};
 auto nearest=render(actorRenderer,actorAssets,frame,read,720,360,true,1);auto adaptive=render(actorRenderer,actorAssets,frame,read,720,360,true,3);
 assert(nearest->pixels[size_t(316)*720+542]==0xff8090a0u);assert(adaptive->pixels[size_t(316)*720+542]!=nearest->pixels[size_t(316)*720+542]);
 auto hidden=std::make_shared<radar::Terrain>(*frame.terrain);hidden->reveal=false;hidden->shade[7*512+7]={-2,-1};frame.terrain=hidden;
 auto masked=render(actorRenderer,actorAssets,frame,read,720,360,true,3);assert(masked->pixels[size_t(337)*720+553]==0xff000000u);
 // Exhaust a command budget using tiny footprints. A failed rebuild may not
 // mark its partial surface as cached or reuse the previous bank under a new key.
 camera::NativeCanvas budgetBacking;assert(budgetBacking.reset(8,8));camera::ViewportCanvas budget;bool rebuild=false;std::vector<uint32_t> previous,recovered;
 assert(budget.prepareTerrain(budgetBacking,nullptr,8,8,0,0,8,8,1,colorprecision::Mode::NativeSteps,3,rebuild)&&rebuild);
 assert(budget.underlayRect(0,0,1,1,0xffffff)&&budget.commitTerrain(previous)&&previous[0]==0xffffffff);
 assert(budget.prepareTerrain(budgetBacking,&previous,8,8,0,0,8,8,2,colorprecision::Mode::NativeSteps,3,rebuild)&&rebuild);
 for(int n=0;n<65536;++n)assert(budget.underlayRect(0,0,1,1,0x334455));assert(!budget.underlayRect(0,0,1,1,0x334455));
 assert(budget.prepareTerrain(budgetBacking,&previous,8,8,0,0,8,8,2,colorprecision::Mode::NativeSteps,3,rebuild)&&rebuild);
 assert(budget.commitTerrain(recovered)&&std::all_of(recovered.begin(),recovered.end(),[](uint32_t p){return p==0xff080c08;}));
 std::puts("PASS: 1:1 terrain equivalence, target-pixel 1.5/2/3/4x TMP cutouts/extras and overlays, one shadow pass, live palette/light, nearest cliff Z, wide-extra culling, transparent margins, dynamic nearest/adaptive modes and final shroud");
}
