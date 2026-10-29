#include "../../external_reader/source/gpu_scene_record.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace external;
using namespace vulkancompute;
static DrawJob draw(std::shared_ptr<const std::vector<uint32_t>> pixels,int x,int y,uint32_t id,float z=20){
 DrawJob d;d.source.pixels=std::move(pixels);d.source.width=d.source.height=2;d.source.depth=std::make_shared<const std::vector<float>>(4,0);
 d.left=float(x);d.top=float(y);d.depthBase=z;d.motion={0x10000+id*0x100,id,0x20000,2,{},true};return d;
}
static std::vector<uint32_t> reference(const FrameJob&f){
 camera::Surface cpu;assert(cpu.reset(f.width,f.height));cpu.color=*f.basePixels;cpu.depth=*f.baseDepth;lighting::State light;light.intensity=1000;light.tint={{1000,1000,1000}};
 for(const auto&d:f.draws){voxel::Image i;i.width=d.source.width;i.height=d.source.height;i.pixels=*d.source.pixels;i.depth=*d.source.depth;i.shadow=d.shadow;i.writeDepth=d.writeDepth;i.skipZ=d.skipZ;
  cpu.blit(i,d.left,d.top,d.scaleX,d.scaleY,d.depthBase,light,d.lift,0,false);
 }
 for(int y=0;y<f.height;++y)for(int x=0;x<f.width;++x){size_t n=size_t(y)*f.width+x;if(x<f.clipLeft||y<f.clipTop||x>=f.clipRight||y>=f.clipBottom){cpu.color[n]=0;continue;}
  if(f.fogAlpha&&(*f.fogAlpha)[n]<127){unsigned a=(*f.fogAlpha)[n];uint32_t c=cpu.color[n]&0xff000000u;for(int shift:{0,8,16})c|=((((cpu.color[n]>>shift)&255)*a+63)/127)<<shift;cpu.color[n]=c;}
 }return cpu.color;
}
int main(int argc,char**argv){
 bool require=argc>1&&std::string(argv[1])=="--require";
 FrameJob f;f.width=64;f.height=48;size_t count=size_t(f.width)*f.height;
 f.basePixels=std::make_shared<const std::vector<uint32_t>>(count,0xff203040u);f.baseDepth=std::make_shared<const std::vector<float>>(count,0);
 f.clipLeft=f.clipTop=1;f.clipRight=63;f.clipBottom=47;f.collectGuides=true;
 f.motion.context=std::make_shared<const int>(1);f.motion.depthFar=-100;f.motion.depthNear=100;f.motion.frame=1;f.motion.tick=100;f.motion.terrainRevision=1;
 auto pixels=std::make_shared<const std::vector<uint32_t>>(4,0xffc06030u);
 f.draws.push_back(draw(pixels,8,7,1));f.draws.push_back(draw(pixels,22,8,2));
 f.draws.push_back(draw(std::make_shared<const std::vector<uint32_t>>(4,0x80604020u),30,15,3));
 auto shadow=draw(std::make_shared<const std::vector<uint32_t>>(4,0xff000000u),45,20,4);shadow.shadow=true;shadow.writeDepth=false;f.draws.push_back(shadow);
 f.draws.push_back(draw(pixels,50,30,5,-20)); // rejected behind terrain
 auto fog=std::make_shared<std::vector<uint8_t>>(count,127);(*fog)[3*64+3]=63;f.fogAlpha=fog;
 // Production recording keeps exact attachment paths separate from painter
 // order and shares no synthetic identity between distinct game instances.
 {
  auto image=std::make_shared<voxel::Image>();image->width=image->height=2;image->pixels=*pixels;
  voxel::Image part=*image;part.shadow=true;image->attachments={part,part};camera::GpuSceneRecord record;
  record.begin(64,48,f.basePixels,f.baseDepth,colorprecision::Mode::NativeSteps);record.motion(f.motion,true);
  assert(record.append(*image,image,8,7,1,1,20,{},1,0,false,f.draws[0].motion));
  assert(record.frame().draws.size()==3);
  assert(record.frame().draws[0].motion.part==std::vector<uint32_t>{0});
  assert(record.frame().draws[1].motion.part==std::vector<uint32_t>{1});
  assert(record.frame().draws[2].motion.part.empty());
 }
 FrameResolve gpu;std::vector<uint32_t> actual;
 if(!gpu.renderFrame(f,actual)){assert(!gpu.guides());std::printf("VULKAN_GUIDES_UNAVAILABLE %s\n",gpu.diagnostics().reason.c_str());return require?1:0;}
 assert(actual==reference(f));auto guides=gpu.guides();assert(guides&&guides->resetHistory&&guides->sequence==1);
 assert(guides->motion.size()==count*2&&guides->depth.size()==count&&guides->confidence.size()==count);
 for(float m:guides->motion)assert(m==0);
 auto at=[](int x,int y){return size_t(y)*64+x;};
 assert(guides->confidence[at(5,5)]==1&&guides->depth[at(5,5)]==.5f);
 assert(guides->confidence[at(8,7)]==1&&std::abs(guides->depth[at(8,7)]-.6f)<1e-6f);
 assert(guides->confidence[at(30,15)]==0&&guides->confidence[at(45,20)]==0);
 assert(guides->confidence[at(3,3)]==0&&guides->confidence[0]==0&&actual[0]==0);
 auto second=f;second.motion.x=2;second.motion.y=1;second.motion.tick+=16;++second.motion.frame;
 second.draws[0].left=7;second.draws[0].top=6;second.draws[1].left=20;
 assert(gpu.renderFrame(second,actual)&&actual==reference(second));guides=gpu.guides();assert(guides&&!guides->resetHistory&&guides->sequence==2);
 assert(guides->motion[at(5,5)*2]==2&&guides->motion[at(5,5)*2+1]==1);
 assert(guides->motion[at(7,6)*2]==1&&guides->motion[at(7,6)*2+1]==1);
 assert(guides->motion[at(20,8)*2]==2&&guides->motion[at(20,8)*2+1]==0);
 // A failed Z test cannot steal the terrain's motion or depth.
 assert(guides->motion[at(50,30)*2]==2&&guides->motion[at(50,30)*2+1]==1&&guides->depth[at(50,30)]==.5f);
 auto unknown=second;unknown.motion.tick+=16;unknown.draws[0].motion.known=false;
 assert(gpu.renderFrame(unknown,actual)&&actual==reference(unknown));guides=gpu.guides();assert(guides&&guides->resetHistory&&guides->confidence[at(7,6)]==0);
 auto changed=second;changed.motion.tick+=32;changed.draws[0].source.pixels=std::make_shared<const std::vector<uint32_t>>(*pixels);
 assert(gpu.renderFrame(changed,actual)&&actual==reference(changed)&&gpu.guides()->resetHistory);
 // Invalid guides never reject an otherwise valid current world frame.
 auto invalid=second;invalid.motion.context.reset();assert(gpu.renderFrame(invalid,actual)&&actual==reference(invalid)&&!gpu.guides());
 auto disabled=second;disabled.collectGuides=false;assert(gpu.renderFrame(disabled,actual)&&actual==reference(disabled)&&!gpu.guides());
 assert(gpu.diagnostics().readbacks==1);
 std::puts("PASS: actual Vulkan motion/depth/control guides, shared-pose independent units, camera vectors, Z rejection, alpha/shadow/fog/clip holds, conservative reset and disabled/raw-frame preservation; one scene submit/readback");
}
