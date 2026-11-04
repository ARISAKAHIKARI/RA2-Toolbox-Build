#include "../../external_reader/source/radar_scene.h"
#include "gpu_voxel_cases.h"
#include <cstdlib>
#include <iostream>

using namespace external;
struct Assets {
 struct ActorArt {bool voxel=false;voxel::Model model;std::array<uint32_t,256> colors{};unsigned palette=0;};
 bool lightReady=true;uint64_t lightEpoch=0;
 bool voxelLightingAvailable()const{return lightReady;}
 uint64_t voxelLightingRevision()const{return lightEpoch;}
 bool gpu=false;int scale=1;unsigned paletteEpoch=0,captures=0,rasters=0,shapeRasters=0;
 voxel::Vpl pages=gpuvoxeltest::vpl();
 bool presentationGPU()const{return gpu;}
 uint64_t revision(){return 1;}
 static size_t cost(const ActorArt&a){return a.model.faces.size()*sizeof(voxel::Face)+sizeof(a);}
 bool palette(bool,std::array<uint32_t,256>&out){for(unsigned i=0;i<256;++i)out[i]=0xff000000u|((i*19&255)<<16)|((i*31&255)<<8)|(i*53&255);return true;}
 void colors(ActorArt&a){a.palette=paletteEpoch;for(unsigned i=0;i<256;++i)a.colors[i]=0xff000000u|(((i*29+paletteEpoch*53)&255)<<16)|(((i*61+paletteEpoch*7)&255)<<8)|((i*103+paletteEpoch*11)&255);}
 bool refreshActorPalette(ActorArt&a,bool&changed){changed=a.palette!=paletteEpoch;if(changed)colors(a);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&pal,voxel::Image&out){
  out={};out.width=60;out.height=30;out.x=-30;out.y=-15;out.flat=true;out.pixels.resize(1800);out.indices.resize(1800);out.depth.assign(1800,0);
  for(int y=0;y<30;++y)for(int x=0;x<60;++x){const size_t at=y*60+x;out.indices[at]=uint8_t(1+(x/4+y/3)%200);out.pixels[at]=pal[out.indices[at]];}
  return true;
 }
 bool overlay(uint32_t,uint32_t,int,voxel::Image&,bool=false){return false;}
 bool shroud(bool fog,int frame,voxel::Image&out){
  out={};out.canvasWidth=out.width=60;out.canvasHeight=out.height=30;out.x=-30;out.y=-15;out.indices.assign(1800,254);
  for(int y=0;y<30;++y)for(int x=0;x<60;++x)if(std::abs(x-29.5)/30.+std::abs(y-14.5)/15.<1.)out.indices[y*60+x]=frame==15?2:uint8_t(fog?98:(x+y+frame)%128);
  return true;
 }
 void buildingPose(uint32_t,uint32_t,visual::Pose&){}
 bool captureActor(uint32_t,uint32_t type,uint32_t,ActorArt&out){++captures;out={};out.voxel=type>=1000;if(out.voxel)out.model=gpuvoxeltest::model();colors(out);return true;}
 bool captureAnimation(uint32_t p,uint32_t t,uint32_t id,uint32_t,uint32_t,ActorArt&out){return captureActor(p,t,id,out);}
 bool renderActor(const ActorArt&a,const visual::Pose&pose,voxel::Image&out){
  if(a.voxel){if(!lightReady)return false;++rasters;const auto angle=visual::fineRadians(pose.bodyFine,pose.body),turret=visual::fineRadians(pose.turretFine,pose.turret);
   if(gpu){auto packet=std::make_shared<gpuvoxel::Prepared>();if(!gpuvoxel::prepare(a.model,angle,a.colors,*packet,turret,3.5f,&pages,255,0,false,scale,&pose.posture))return false;
    out={};out.gpuRaster=packet;out.width=packet->width;out.height=packet->height;out.x=packet->x;out.y=packet->y;out.rasterScale=packet->rasterScale;out.depthBottom=packet->depthBottom;out.objectPalette=true;
   }else if(!voxel::render(a.model,angle,a.colors,out,turret,3.5f,&pages,255,0,false,scale,&pose.posture))return false;
   out.depth.clear();out.writeDepth=false;return true;
  }
  ++shapeRasters;out={};out.width=11;out.height=9;out.x=-5;out.y=-8;out.objectPalette=true;out.writeDepth=true;out.pixels.resize(99);out.indices.resize(99);
  for(int y=0;y<9;++y)for(int x=0;x<11;++x){auto at=y*11+x;out.indices[at]=uint8_t(x==5?244:20+(x+y+pose.frame)%11);out.pixels[at]=(x+y)%5?a.colors[out.indices[at]]:0;}
  auto shadow=out;shadow.attachments.clear();shadow.shadow=true;shadow.flat=true;shadow.depthBias=-3;shadow.y+=4;out.attachments.push_back(std::move(shadow));return true;
 }
};
int main(int argc,char**argv){
 const bool required=argc>1&&std::string(argv[1])=="--require";
 auto read=[](uint32_t,void*,size_t){return false;};uint64_t channels=0,maskPixels=0;unsigned frames=0,used=0,maxError=0;
 for(bool refined:{true,false})for(bool masked:{false,true})for(double ratio:{1.,1.5,2.}){
  auto terrain=std::make_shared<radar::Terrain>();terrain->geo={8,8,6,7,8,8,1,0x10000,0x20000,512*512,{8,8,0,0,8,8},8,8};terrain->reveal=!masked;
  terrain->colors.resize(512*512);terrain->art.resize(512*512);terrain->shade.resize(512*512);
  const size_t cell=7*512+7;terrain->colors[cell]=0x204060;terrain->art[cell].type=1;terrain->art[cell].sourceColor=0x204060;terrain->shade[cell]={0,0};
  radar::Frame frame;frame.terrain=terrain;frame.tick=100;frame.frame=77;
  for(unsigned n=0;n<3;++n){radar::Marker m;m.x=7*256-int(n)*42;m.y=7*256-int(n)*93;m.object=0x10000+n*128;m.id=10+n;m.type=n?1000+n:1;m.owner=0x20000;m.kind=2;m.pose.voxel=n!=0;m.pose.bodyFine=int(n)*31;m.pose.turretFine=67-int(n)*11;frame.markers.push_back(m);}
  Assets cpu,gpu;cpu.scale=gpu.scale=refined?int(std::ceil(ratio)):1;gpu.gpu=true;radar::SceneRenderer<Assets> a,b;b.guideEnabled(true,123);
  const int w=int(240*ratio),h=int(120*ratio);const radar::SceneRegion crop{-3.25,-1.5,240,120,true};
  auto compare=[&](){
   xbrzadapter::gpuPresentation(false);auto expected=a.render(frame,w,h,read,cpu,crop,refined?3:1,true,true,true);assert(expected);
   xbrzadapter::gpuPresentation(true);auto actual=b.render(frame,w,h,read,gpu,crop,refined?3:1,refined,true,true);assert(actual);
   if(required&&!xbrzadapter::gpuUsed()){std::cerr<<"GPU renderer unavailable: "<<(xbrzadapter::gpuState().resolve?xbrzadapter::gpuState().resolve->diagnostics().reason:"not submitted")<<std::endl;std::abort();}
   used+=xbrzadapter::gpuUsed();assert(actual->pixels.size()==expected->pixels.size());
   for(size_t i=0;i<actual->pixels.size();++i)for(unsigned shift:{0u,8u,16u,24u}){unsigned x=(actual->pixels[i]>>shift)&255,y=(expected->pixels[i]>>shift)&255,error=x>y?x-y:y-x;maxError=std::max(maxError,error);
    if(error>1){std::cerr<<"SceneRenderer mismatch masked="<<masked<<" ratio="<<ratio<<" frame="<<frames<<" pixel="<<i<<" actual="<<std::hex<<actual->pixels[i]<<" expected="<<expected->pixels[i]<<std::dec<<std::endl;std::abort();}++channels;}
   const std::vector<uint8_t>*ma=nullptr,*mb=nullptr;assert(a.feedbackVisibility(ma)&&b.feedbackVisibility(mb));if(masked){assert(ma&&mb&&*ma==*mb);maskPixels+=ma->size();}
   if(xbrzadapter::gpuUsed()){assert(xbrzadapter::gpuState().resolve->diagnostics().readbacks==1);assert(actual->guides&&actual->guides->width==w&&actual->guides->height==h&&actual->guides->depth.size()==actual->pixels.size());}
   else assert(!actual->guides);
   ++frames;return actual;
  };
  auto first=compare();assert(a.materialsReady()&&b.materialsReady());const auto captures=gpu.captures;
  const auto rasters=gpu.rasters;auto warm=compare();assert(warm->pixels==first->pixels&&gpu.captures==captures&&gpu.rasters==rasters);
  ++cpu.lightEpoch;++gpu.lightEpoch;compare();assert(gpu.rasters>rasters);
  gpu.lightReady=false;assert(!b.render(frame,w,h,read,gpu,crop,refined?3:1,refined,true,true));gpu.lightReady=true;
  ++cpu.paletteEpoch;++gpu.paletteEpoch;auto relit=compare();assert(relit->pixels!=warm->pixels&&gpu.captures==captures);
  frame.markers[1].pose.bodyFine=91;auto turned=compare();assert(turned->pixels!=relit->pixels&&gpu.captures==captures);
  if(masked){auto changed=std::make_shared<radar::Terrain>(*terrain);changed->shade[cell]={15,0};frame.terrain=changed;auto covered=compare();assert(covered->pixels!=turned->pixels);}
  // CPU/GPU representation switches retain decoded VXL/HVA source geometry.
  gpu.gpu=false;xbrzadapter::gpuPresentation(true);auto fallback=b.render(frame,w,h,read,gpu,crop,refined?3:1,refined,true,true);assert(fallback&&gpu.captures==captures);
 }
 std::cout<<"PASS actual SceneRenderer CPU/GPU used="<<used<<"/"<<frames<<" channel_comparisons="<<channels<<" max_error="<<maxError<<" mask_pixels="<<maskPixels<<"; native1x/1.5x/2x, rawterrain+SHPxBRZ, deferredVXL, fog, livepalette/pose/sameclock and representationtoggle source retention"<<std::endl;
}
