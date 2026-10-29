#include "../../external_reader/source/vulkan_area.h"
#include "gpu_shroud_cases.h"
#include "../../external_reader/source/native_canvas.h"
#include "../../external_reader/source/radar_scene.h"
#include "gpu_voxel_cases.h"
#include <cassert>
#include <cstdio>
#include <new>
#include <random>
#include <string>
using namespace external;
static std::shared_ptr<const std::vector<uint32_t>> page(std::mt19937&r){auto p=std::make_shared<std::vector<uint32_t>>(gpuarea::PagePixels);for(auto&v:*p)v=r();return p;}
namespace {
struct MiniAssets {
 struct ActorArt {bool voxel=false;voxel::Model model;std::array<uint32_t,256>colors{};};unsigned epoch=0;
 uint64_t revision(){return 1;}static size_t cost(const ActorArt&a){return sizeof a+a.model.faces.size()*sizeof(voxel::Face);}
 void colors(ActorArt&a){for(unsigned n=0;n<256;++n)a.colors[n]=0xff000000u|((n*13+epoch*11)&255)<<16|((n*7+epoch*23)&255)<<8|((n*29+epoch*7)&255);}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xff207040+epoch*0x000a0301);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&p,voxel::Image&i){i={};i.width=60;i.height=30;i.x=-30;i.y=-15;i.pixels.assign(1800,0);i.depth.assign(1800,0);for(int y=0;y<30;++y)for(int x=0;x<60;++x)if(std::abs(x-29.5)/30.+std::abs(y-14.5)/15.<1.)i.pixels[y*60+x]=p[1]+uint32_t(x*3+y);return true;}
 bool overlay(uint32_t,uint32_t,int frame,voxel::Image&i,bool=false){i={};i.width=i.height=13;i.x=i.y=-6;i.pixels.assign(169,0xff905020u+frame*0x00040103);i.flat=true;return true;}
 bool shroud(bool f,int n,voxel::Image&i){return shroudcases::shape(f,n,i);}
 void buildingPose(uint32_t,uint32_t,visual::Pose&){}
 bool captureActor(uint32_t,uint32_t type,uint32_t,ActorArt&a){a.voxel=type>=1000;if(a.voxel)a.model=gpuvoxeltest::model();colors(a);return true;}
 bool captureAnimation(uint32_t p,uint32_t t,uint32_t id,uint32_t,uint32_t,ActorArt&a){return captureActor(p,t,id,a);}
 bool refreshActorPalette(ActorArt&a,bool&changed){auto old=a.colors;colors(a);changed=old!=a.colors;return true;}
 bool renderActor(const ActorArt&a,const visual::Pose&pose,voxel::Image&i){if(a.voxel)return voxel::render(a.model,visual::fineRadians(pose.bodyFine,pose.body),a.colors,i,0,0,nullptr,255,0,false,1,&pose.posture);i={};i.width=19;i.height=25;i.x=-9;i.y=-24;i.pixels.assign(475,a.colors[20]);i.depth.assign(475,7);return true;}
};
uint64_t production(bool required){
 auto read=[](uint32_t,void*,size_t){return false;};uint64_t channels=0;
 for(bool masked:{false,true}){
  auto terrain=std::make_shared<radar::Terrain>();terrain->geo={8,8,6,7,8,8,1,0x10000,0x20000,512*512,{8,8,0,0,8,8},8,8};terrain->reveal=!masked;terrain->colors.resize(512*512);terrain->art.resize(512*512);terrain->shade.resize(512*512);const size_t cell=7*512+7;terrain->colors[cell]=0x207040;terrain->art[cell].type=1;terrain->art[cell].sourceColor=0x207040;terrain->shade[cell]={0,0};
  radar::Frame frame;frame.terrain=terrain;frame.tick=100;frame.frame=77;MiniAssets ca,ga;radar::SceneRenderer<MiniAssets>cpu,gpu;
  auto compare=[&](int w,int h){xbrzadapter::gpuPresentation(false);auto a=cpu.render(frame,w,h,read,ca);assert(a);xbrzadapter::gpuPresentation(true);auto b=gpu.render(frame,w,h,read,ga);assert(b);if(required)assert(xbrzadapter::gpuUsed());for(size_t n=0;n<a->pixels.size();++n)for(int s:{0,8,16,24}){assert(std::abs(int((a->pixels[n]>>s)&255)-int((b->pixels[n]>>s)&255))<=1);++channels;}if(xbrzadapter::gpuUsed())assert(xbrzadapter::gpuState().area->diagnostics().readbacks<=1);return b;};
  auto first=compare(160,80);const bool firstGPU=xbrzadapter::gpuUsed();auto warm=compare(160,80);if(firstGPU&&xbrzadapter::gpuUsed())assert(first==warm);if(xbrzadapter::gpuUsed())assert(xbrzadapter::gpuState().area->diagnostics().dispatches==0&&xbrzadapter::gpuState().area->diagnostics().readbacks==0&&xbrzadapter::gpuState().area->prefixBuilds()==0&&xbrzadapter::gpuState().area->diagnostics().uploads==0);
  for(unsigned n=0;n<2;++n){radar::Marker m;m.x=7*256-int(n)*35;m.y=7*256-int(n)*71;m.object=0x40000+n*256;m.id=n+1;m.type=n?1000:1;m.owner=0x20000;m.kind=2;m.pose.voxel=n!=0;m.pose.bodyFine=27;frame.markers.push_back(m);}
  compare(160,80);++ca.epoch;++ga.epoch;compare(160,80);frame.markers[1].pose.bodyFine=93;compare(160,80);compare(319,31);
  if(masked){auto next=std::make_shared<radar::Terrain>(*terrain);next->shade[cell]={15,0};frame.terrain=next;compare(160,80);}
  // Disabling the option restores the original cached CPU route, including
  // a GPU-only native mask whose CPU alpha vector was never requested.
  xbrzadapter::gpuPresentation(false);auto disabled=gpu.render(frame,160,80,read,ga);assert(disabled&&disabled->pixels.size()==12800);
 }
 xbrzadapter::gpuPresentation(false);return channels;
}
}
int main(int argc,char**argv){bool required=argc>1&&std::string(argv[1])=="--require";vulkancompute::AreaResolve gpu;std::mt19937 random(20261029);gpuarea::Plan p;p.nativeWidth=513;p.nativeHeight=389;p.width=73;p.height=51;p.spanX=512.625;p.spanY=388.375;p.background=0x053d2811;
 for(uint32_t y=0;y<4;++y)for(uint32_t x=0;x<5;++x)if((x+2*y)%4!=2)p.pages.push_back({x,y,page(random)});
 std::vector<uint32_t>out{0x12345678};auto invalid=p;invalid.width=0;assert(!gpu.render(invalid,out)&&out==std::vector<uint32_t>{0x12345678});
 if(!gpu.render(p,out)){std::printf("VULKAN_UNAVAILABLE %s\n",gpu.diagnostics().reason.c_str());if(!required)production(false);return required?1:0;}
 uint64_t checks=0,frames=0;unsigned maxError=0;auto check=[&](){std::vector<uint32_t>expected;assert(gpuarea::reference(p,expected));assert(out.size()==expected.size());for(size_t n=0;n<out.size();++n)for(int s:{0,8,16,24}){unsigned error=unsigned(std::abs(int((out[n]>>s)&255)-int((expected[n]>>s)&255)));maxError=std::max(maxError,error);if(error>1){std::fprintf(stderr,"Mismatch frame%llu at%zu shift%d actual%08x expected%08x\n",(unsigned long long)frames,n,s,out[n],expected[n]);assert(false);}++checks;}assert(gpu.diagnostics().readbacks==1);++frames;};check();assert(gpu.prefixBuilds()==p.pages.size());
 for(auto dims:std::vector<std::array<int,2>>{{1,1},{7,3},{31,17},{128,63},{511,389},{731,519},{23,519},{731,23},{160,80}}){p.width=dims[0];p.height=dims[1];assert(gpu.render(p,out));assert(gpu.prefixBuilds()==0&&gpu.diagnostics().uploads==0&&gpu.diagnostics().dispatches==1);check();}
 for(int n=0;n<12;++n){p.spanX=513.-double(n+1)/32;p.spanY=389.-double(n+1)/37;assert(gpu.render(p,out));assert(gpu.prefixBuilds()==0&&gpu.diagnostics().uploads==0);check();}
 p.pages[1].pixels=page(random);assert(gpu.render(p,out));assert(gpu.prefixBuilds()==1&&gpu.diagnostics().uploads==1);check();
 // A new shared owner at an identical vector address must rebuild its slot.
 alignas(std::vector<uint32_t>) unsigned char storage[sizeof(std::vector<uint32_t>)];auto reused=[&](){auto image=page(random);auto*v=new(storage) std::vector<uint32_t>(*image);return std::shared_ptr<const std::vector<uint32_t>>(v,[](const auto*q){const_cast<std::vector<uint32_t>*>(q)->~vector();});};p.pages[2].pixels=reused();assert(gpu.render(p,out));assert(gpu.prefixBuilds()==1);check();p.pages[2].pixels.reset();p.pages[2].pixels=reused();assert(gpu.render(p,out));assert(gpu.prefixBuilds()==1);check();
 p.pages.erase(p.pages.begin()+3);assert(gpu.render(p,out));assert(gpu.prefixBuilds()==0);check();
 auto fog=std::make_shared<std::vector<uint8_t>>(size_t(p.width)*p.height);for(size_t n=0;n<fog->size();++n)(*fog)[n]=uint8_t(n%128);p.fog=fog;assert(gpu.render(p,out));assert(gpu.prefixBuilds()==0);check();
 // Native SHROUD/FOG raster stays GPU-resident and shares one final fence.
 auto g=shroudcases::geometry();auto cells=shroudcases::cells(g);gpushroud::Builder builder;std::shared_ptr<const gpushroud::Plan>mask;assert(builder.build(g,cells,p.width,p.height,p.spanX,p.spanY,true,[](bool f,int n,voxel::Image&i){return shroudcases::shape(f,n,i);},mask));p.fog.reset();p.mask=mask;std::vector<uint8_t>alpha;assert(gpushroud::reference(*mask,alpha));auto referencePlan=p;referencePlan.mask.reset();referencePlan.fog=std::make_shared<const std::vector<uint8_t>>(alpha);assert(gpu.render(p,out));auto saved=p;p=referencePlan;check();p=saved;assert(gpu.render(p,out));assert(gpu.prefixBuilds()==0&&gpu.diagnostics().uploads==0&&gpu.diagnostics().dispatches==1);p=referencePlan;check();
 // Endpoints exactly at a native page edge must clamp the1ulp product
 // overshoot:2688/19*19 otherwise addresses nonexistent pagecolumn21.
 p={};p.nativeWidth=2688;p.nativeHeight=128;p.width=19;p.height=3;p.spanX=2688;p.spanY=128;p.pages={{0,0,page(random)},{20,0,page(random)}};assert(gpu.render(p,out));check();
 // Production mutable canvas versioning: no GPU cache pins native pages or
 // prevents the previous bank from releasing its bounded page budget.
 camera::NativeCanvas canvas;auto budget=std::make_shared<camera::NativeCanvas::Budget>();budget->maximum=8;assert(canvas.reset(256,128,budget));
 canvas.underlayRect(0,0,256,128,0x223344);gpuarea::Plan cp;assert(canvas.areaPlan(43,17,cp,256,128));assert(gpu.render(cp,out));std::vector<uint32_t>native;assert(canvas.present(43,17,native,256,128));assert(out==native);cp={};size_t resident=budget->pages;
 voxel::Image image;image.width=image.height=2;image.flat=true;image.writeDepth=false;image.pixels.assign(4,0xffdd3322);canvas.blit(image,1,1,1,1,0,lighting::State{});assert(budget->pages==resident);assert(canvas.areaPlan(43,17,cp,256,128));assert(gpu.render(cp,out));assert(gpu.prefixBuilds()==1&&gpu.diagnostics().uploads==1);assert(canvas.present(43,17,native,256,128));assert(out==native);cp={};
 canvas={};assert(budget->pages==0);frames+=2;checks+=uint64_t(43)*17*4*2; // weakGPUaliases do not retain either bank
 const auto before=out;invalid=p;invalid.pages.push_back(invalid.pages.front());assert(!gpu.render(invalid,out)&&out==before);invalid=p;invalid.spanX=std::numeric_limits<double>::quiet_NaN();assert(!gpu.render(invalid,out)&&out==before);
 const uint64_t sceneChannels=production(required);
 std::printf("PASS actual GPU sparse area frames=%llu channels=%llu max_error=%u; shrink/upscale/fractional/sparse/deleted/owner-reuse/current native fog, warm0uploads0prefixbuilds onefinalreadback; production_scene_channels=%llu software=%u device=%s\n",(unsigned long long)frames,(unsigned long long)checks,maxError,(unsigned long long)sceneChannels,unsigned(gpu.diagnostics().softwareDevice),gpu.diagnostics().device.c_str());
}
