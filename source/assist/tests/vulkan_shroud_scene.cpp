#include "gpu_shroud_cases.h"
#include "../../external_reader/source/vulkan_scene.h"
#include "../../external_reader/source/camera_raster.h"
#include <cassert>
#include <cstdio>
#include <string>
int main(int argc,char**argv){
 using namespace external;const bool required=argc>1&&std::string(argv[1])=="--require";
 vulkancompute::FrameResolve gpu;gpushroud::Builder builder;const auto geo=shroudcases::geometry();const auto cells=shroudcases::cells(geo);
 auto sprite=std::make_shared<voxel::Image>();sprite->width=20;sprite->height=16;sprite->x=-2;sprite->y=-3;sprite->flat=true;sprite->objectPalette=true;sprite->depthBias=1.5f;
 sprite->pixels.resize(320);sprite->indices.resize(320);sprite->depth.resize(320);
 for(int y=0;y<16;++y)for(int x=0;x<20;++x){size_t at=size_t(y)*20+x;unsigned a=(x+y)%9==0?0:((x+y)%3==0?128:255);
  sprite->pixels[at]=(a<<24)|((unsigned((x*13+y*7)&255)*a/255)<<16)|((unsigned((x*3+y*19)&255)*a/255)<<8)|(unsigned((x*17+y*5)&255)*a/255);
  sprite->indices[at]=(x+y)%5==0?243:uint8_t(x+y+2);sprite->depth[at]=(x+y)%7==0?std::numeric_limits<float>::quiet_NaN():float(y)*.5f;
 }
 std::shared_ptr<const voxel::Image> immutable=sprite;
 lighting::State light;light.intensity=800;light.tint={{90,110,70}};camera::Light gains(light);
 uint64_t frames=0,pixels=0,maskBytes=0;
 for(const auto&c:shroudcases::cases()){
  std::shared_ptr<const gpushroud::Plan>plan;auto load=[](bool f,int n,voxel::Image&i){return shroudcases::shape(f,n,i);};
  assert(builder.build(geo,cells,c.w,c.h,c.spanX,c.spanY,c.fog,load,plan,c.x,c.y));radar::shroudmask::Raster mask;
  assert(mask.build(geo,cells,c.w,c.h,c.spanX,c.spanY,c.fog,load,c.x,c.y));
  auto color=std::make_shared<std::vector<uint32_t>>(size_t(c.w)*c.h);auto depth=std::make_shared<std::vector<float>>(color->size(),-std::numeric_limits<float>::infinity());
  for(size_t n=0;n<color->size();++n){unsigned x=unsigned(n%c.w),y=unsigned(n/c.w);(*color)[n]=(x+y)%19==0?0:0xff000000u|(((x*7+y*3)&255)<<16)|(((x*13+y*5)&255)<<8)|((x*11+y*17)&255);if((x+y)%7==0)(*depth)[n]=50;}
  vulkancompute::FrameJob frame;frame.width=c.w;frame.height=c.h;frame.basePixels=color;frame.baseDepth=depth;frame.maskPlan=plan;
  frame.clipLeft=1;frame.clipTop=2;frame.clipRight=c.w-1;frame.clipBottom=c.h-2;
  const float ax=std::min(15.25f,c.w/4.f),ay=std::min(17.5f,c.h/4.f),base=30;
  vulkancompute::DrawJob draw;draw.source={std::shared_ptr<const std::vector<uint32_t>>(immutable,&immutable->pixels),std::shared_ptr<const std::vector<float>>(immutable,&immutable->depth),std::shared_ptr<const std::vector<uint8_t>>(immutable,&immutable->indices),sprite->width,sprite->height};
  draw.left=ax+sprite->x;draw.top=ay+sprite->y;draw.depthBase=base+sprite->depthBias;draw.originY=sprite->y;draw.depthBottom=sprite->y+sprite->height-1;draw.flat=true;draw.objectPalette=true;std::copy_n(gains.gains,3,draw.gains.begin());frame.draws.push_back(draw);
  camera::Surface cpu;assert(cpu.reset(c.w,c.h));cpu.color=*color;cpu.depth=*depth;cpu.blit(*sprite,ax,ay,1,1,base,light);
  for(int y=0;y<c.h;++y)for(int x=0;x<c.w;++x)if(x<frame.clipLeft||y<frame.clipTop||x>=frame.clipRight||y>=frame.clipBottom)cpu.color[size_t(y)*c.w+x]=0;
  mask.apply(cpu.color);
  std::vector<uint32_t> actual;std::vector<uint8_t>actualMask;
  if(!gpu.renderFrame(frame,actual,&actualMask)){std::printf("VULKAN_UNAVAILABLE %s\n",gpu.diagnostics().reason.c_str());return required?1:0;}
  assert(actualMask==mask.alpha);assert(actual==cpu.color);assert(gpu.diagnostics().readbacks==1);++frames;pixels+=actual.size();maskBytes+=actualMask.size();
  assert(gpu.renderFrame(frame,actual,&actualMask));assert(actualMask==mask.alpha&&actual==cpu.color);assert(gpu.diagnostics().uploads==0&&gpu.diagnostics().uploadedBytes==0&&gpu.diagnostics().readbacks==1);++frames;pixels+=actual.size();maskBytes+=actualMask.size();
 }
 std::printf("PASS Vulkan shroud FrameResolve frames=%llu exact_color_pixels=%llu exact_mask_bytes=%llu warm_texture_uploads=0 final_readbacks=1; baseZ/spriteZ/indices/emissive/alpha0/clip/material-mask-atlas isolation pass software=%u\n",(unsigned long long)frames,(unsigned long long)pixels,(unsigned long long)maskBytes,unsigned(gpu.diagnostics().softwareDevice));
}
