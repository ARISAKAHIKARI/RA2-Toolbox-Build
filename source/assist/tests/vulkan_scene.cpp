#include "../../external_reader/source/vulkan_scene.h"
#include "../../external_reader/source/camera_raster.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <random>
using namespace external;
using namespace external::vulkancompute;
static Texture texture(const std::shared_ptr<const voxel::Image>&image){Texture t;t.width=image->width;t.height=image->height;t.pixels=std::shared_ptr<const std::vector<uint32_t>>(image,&image->pixels);if(!image->depth.empty())t.depth=std::shared_ptr<const std::vector<float>>(image,&image->depth);if(!image->indices.empty())t.indices=std::shared_ptr<const std::vector<uint8_t>>(image,&image->indices);return t;}
int main(int argc,char**argv){
 const bool required=argc>1&&std::string(argv[1])=="--require";FrameResolve gpu;std::mt19937 random(20261029);uint64_t checked=0;unsigned maxError=0,frames=0;
 for(int mode=0;mode<4;++mode){
  FrameJob frame;frame.width=173;frame.height=109;const size_t count=size_t(frame.width)*frame.height;
  auto background=std::make_shared<std::vector<uint32_t>>(count);auto ground=std::make_shared<std::vector<float>>(count);auto fog=std::make_shared<std::vector<uint8_t>>(count);
  for(size_t n=0;n<count;++n){(*background)[n]=0xff000000u|(random()&0xffffffu);(*ground)[n]=float(int(n%7)-20);(*fog)[n]=uint8_t((n%17)?127:n%128);}
  frame.basePixels=background;frame.baseDepth=ground;frame.fogAlpha=fog;frame.clipLeft=3;frame.clipTop=2;frame.clipRight=169;frame.clipBottom=105;
  camera::Surface cpu;assert(cpu.reset(frame.width,frame.height));cpu.color=*background;cpu.depth=*ground;cpu.precision(mode&1?colorprecision::Mode::SmoothRGB:colorprecision::Mode::NativeSteps);
  for(int n=0;n<53;++n){auto image=std::make_shared<voxel::Image>();image->width=9+int(random()%25);image->height=8+int(random()%23);image->rasterScale=mode>=2?2:1;image->x=int(random()%7)-3;image->y=int(random()%7)-3;image->depthBottom=image->y+image->height+3;image->objectPalette=true;image->flat=n%3!=0;image->shadow=n%7==0;image->writeDepth=n%5!=0;image->pixels.resize(size_t(image->width)*image->height);image->indices.resize(image->pixels.size());if(n%4==0)image->depth.resize(image->pixels.size());
   for(size_t p=0;p<image->pixels.size();++p){unsigned a=p%13==0?0:64+random()%192;image->pixels[p]=(a<<24)|((random()%(a+1))<<16)|((random()%(a+1))<<8)|(random()%(a+1));image->indices[p]=uint8_t(235+random()%20);if(!image->depth.empty())image->depth[p]=p%11==0?std::numeric_limits<float>::quiet_NaN():float(int(random()%15)-7);}
   lighting::State light;light.intensity=int(random()%2001);light.tint={{700+int(random()%1000),700+int(random()%1000),700+int(random()%1000)}};camera::Light transform(light,mode&1?colorprecision::Mode::SmoothRGB:colorprecision::Mode::NativeSteps);
   DrawJob draw;draw.source=texture(image);draw.left=float(int(random()%175)-20)+.125f;draw.top=float(int(random()%110)-15)+.375f;draw.scaleX=mode>=2?.65f:1.f;draw.scaleY=mode>=2?.8f:1.f;draw.depthBase=float(int(random()%30)-15);draw.originY=image->y;draw.depthBottom=image->depthBottom;draw.rasterScale=image->rasterScale;draw.lift=2;for(int c=0;c<3;++c)draw.gains[c]=transform.gains[c];draw.shadow=image->shadow;draw.flat=image->flat;draw.writeDepth=image->writeDepth;draw.objectPalette=image->objectPalette;draw.smoothRGB=mode&1;draw.sampling=mode>=2?Sampling::Bilinear:Sampling::Nearest;
   float sx=draw.scaleX*image->rasterScale,sy=draw.scaleY*image->rasterScale,ax=draw.left-image->x*draw.scaleX,ay=draw.top-image->y*draw.scaleY;
   cpu.blit(*image,ax,ay,sx,sy,draw.depthBase,light,draw.lift,0,mode>=2);frame.draws.push_back(draw);
  }
  for(int y=0;y<frame.height;++y)for(int x=0;x<frame.width;++x){size_t n=size_t(y)*frame.width+x;if(x<frame.clipLeft||x>=frame.clipRight||y<frame.clipTop||y>=frame.clipBottom){cpu.color[n]=0;continue;}uint32_t pixel=cpu.color[n];unsigned alpha=(*fog)[n];if(alpha<127&&(pixel>>24)){uint32_t value=pixel&0xff000000u;for(int s:{0,8,16})value|=((((pixel>>s)&255)*alpha+63)/127)<<s;cpu.color[n]=value;}}
  std::vector<uint32_t> actual{0x12345678};if(!gpu.renderFrame(frame,actual)){std::cout<<"VULKAN_SCENE_UNAVAILABLE "<<gpu.diagnostics().reason<<std::endl;return required?1:0;}
  assert(actual.size()==cpu.color.size());for(size_t n=0;n<actual.size();++n)for(unsigned shift:{0u,8u,16u,24u}){unsigned a=(actual[n]>>shift)&255,b=(cpu.color[n]>>shift)&255,error=a>b?a-b:b-a;maxError=std::max(maxError,error);if(error>1){std::cerr<<"Mismatch mode="<<mode<<" at="<<n<<" shift="<<shift<<" GPU="<<std::hex<<actual[n]<<" CPU="<<cpu.color[n]<<std::dec<<std::endl;return 2;}++checked;}
  assert(gpu.diagnostics().readbacks==1);++frames;
  auto original=actual;assert(gpu.renderFrame(frame,actual)&&actual==original);assert(gpu.diagnostics().uploads==0&&gpu.diagnostics().uploadedBytes==0&&gpu.diagnostics().readbacks==1);++frames;
 }
 std::cout<<"PASS GPU whole-frame nearest+bilinear: frames="<<frames<<" channel_checks="<<checked<<" max_error="<<maxError<<" warm_uploads=0 final_readbacks=1; native1x light/depth/NaN/shadow/alpha/fog/clip"<<std::endl;
}
