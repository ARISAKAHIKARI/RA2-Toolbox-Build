#include "../../external_reader/source/viewport_canvas.h"
#include <cassert>
#include <cstdio>
using namespace external;
static voxel::Image sprite(uint32_t color,bool shadow=false){voxel::Image i;i.width=i.height=3;i.x=-1;i.y=-2;i.flat=true;i.shadow=shadow;i.pixels.assign(9,color);i.indices.assign(9,241);i.objectPalette=true;return i;}
int main(int argc,char**argv){
 const bool gpuRequired=argc>1&&std::string(argv[1])=="--require";
 std::unique_ptr<vulkancompute::FrameResolve> device;if(gpuRequired)device=std::make_unique<vulkancompute::FrameResolve>();
 uint64_t gpuComparisons=0;unsigned gpuMaxError=0;
 auto base=std::make_shared<const std::vector<uint32_t>>(24*18,0xff416283);auto depth=std::make_shared<const std::vector<float>>(24*18,-std::numeric_limits<float>::infinity());
 size_t cases=0;
 for(auto precision:{colorprecision::Mode::NativeSteps,colorprecision::Mode::SmoothRGB})for(bool after:{false,true})for(float ratio:{1.f,1.5f,2.f}){
  auto image=std::make_shared<voxel::Image>(sprite(0x80604020));image->shadowsAfter=after;image->depthBias=2;image->depthBottom=4;for(size_t n=0;n<image->indices.size();++n)image->indices[n]=n%3==0?1:241;image->pixels[4]=0;image->indices[4]=0;
  image->attachments={sprite(0xff000000,true),sprite(0xffa02060)};image->attachments[0].airborneShadow=true;image->attachments[0].y=1;image->attachments[1].x=2;
  lighting::State light;light.intensity=700;light.tint={{900,1100,800}};
  camera::GpuSceneRecord record;record.begin(24,18,base,depth,precision);assert(record.append(*image,image,9,8,ratio,ratio,10,light,1,2,true));
  assert(record.valid()&&record.frame().draws.size()==3);const auto&draw=record.frame().draws[after?0:1];assert(draw.source.pixels.get()==&image->pixels&&draw.source.indices.get()==&image->indices&&draw.depthBase==12&&draw.depthBottom==4);
  if(ratio>1){assert(draw.reconstructionSource&&!draw.reconstructed&&draw.reconstructionFactor==2&&draw.reconstructedWidth==6);camera::Light transform(light,precision);for(size_t n=0;n<image->pixels.size();++n)assert((*draw.reconstructionSource)[n]==transform.apply(image->pixels[n],image->indices[n],true));}
  assert(record.frame().draws[after?1:0].shadow&&record.frame().draws[2].source.pixels.get()==&image->attachments[1].pixels);
  camera::Surface expected;assert(expected.reset(24,18));expected.precision(precision);expected.color=*base;expected.depth=*depth;expected.blit(*image,9,8,ratio,ratio,10,light,1,2,true);
  record.annotationDot(12,13,0x203040);for(int y=12;y<15;++y)for(int x=11;x<14;++x)expected.color[size_t(y)*24+x]=0xff203040;
  if(device){std::vector<uint32_t> actual;if(!device->renderFrame(record.frame(),actual)){std::fprintf(stderr,"GPU record unavailable: %s\n",device->diagnostics().reason.c_str());return 1;}assert(actual.size()==expected.color.size()&&device->diagnostics().readbacks==1);
   for(size_t at=0;at<actual.size();++at)for(int shift:{0,8,16,24}){unsigned error=unsigned(std::abs(int((actual[at]>>shift)&255)-int((expected.color[at]>>shift)&255)));gpuMaxError=std::max(gpuMaxError,error);if(error>1){std::fprintf(stderr,"GPU record mismatch case=%zu xy=(%zu,%zu) shift=%d expected=%08x actual=%08x\n",cases,at%24,at/24,shift,expected.color[at],actual[at]);return 2;}++gpuComparisons;}
  }
  std::weak_ptr<const voxel::Image> retained=image;image.reset();assert(!retained.expired());camera::Surface replay;assert(record.replay(replay));assert(replay.color==expected.color&&replay.depth==expected.depth);++cases;
 }
 if(device){std::printf("PASS: actual GPU recorder scene cases=%zu channel_checks=%llu max_error=%u, raw native SHP xBRZ+ordered shadows+annotations, one final readback; software device is not hardware performance proof\n",cases,(unsigned long long)gpuComparisons,gpuMaxError);device.reset();}
 // Lit native SHP inputs persist without CPU xBRZ output construction, and
 // changed light invalidates only that source while retaining original art.
 {
  auto editable=std::make_shared<voxel::Image>(sprite(0xff416283));editable->indices.assign(9,1);std::shared_ptr<const voxel::Image> image=editable;camera::GpuSceneRecord record;lighting::State light;light.intensity=1000;light.tint={{1000,1000,1000}};
  auto draw=[&]{record.begin(24,18,base,depth,colorprecision::Mode::SmoothRGB);assert(record.append(*image,image,9,8,2,2,10,light,1,0,true));return record.frame().draws.front().reconstructionSource;};
  auto first=draw(),warm=draw();assert(first&&first==warm&&record.frame().draws.front().source.pixels.get()==&image->pixels);
  light.intensity=700;auto changed=draw();assert(changed&&changed!=warm&&*changed!=*warm);
  camera::GpuSceneRecord malformed;malformed.begin(24,18,base,std::make_shared<const std::vector<float>>(1),colorprecision::Mode::NativeSteps);camera::Surface rejected;assert(!malformed.valid()&&!malformed.replay(rejected));
 }
 // Native terrain may stay as a GPU plan; failed GPU work invokes the retained
 // CPU background callback once per replay, then reproduces original sprites.
 int fallbackCalls=0;auto empty=std::make_shared<const std::vector<uint32_t>>();
 auto tiles=std::make_shared<std::vector<vulkancompute::TileJob>>(1);(*tiles)[0].pixels=base;(*tiles)[0].sourceWidth=24;(*tiles)[0].sourceHeight=18;(*tiles)[0].width=24;(*tiles)[0].height=18;
 camera::GpuSceneRecord raw;raw.begin(24,18,empty,depth,colorprecision::Mode::NativeSteps,tiles,[&](std::vector<uint32_t>&out){++fallbackCalls;out=*base;return true;});
 assert(raw.valid()&&!raw.frame().basePixels&&raw.frame().baseTiles.size()==1&&fallbackCalls==0);camera::Surface fallback;assert(raw.replay(fallback)&&fallbackCalls==1&&fallback.color==*base);
 // Viewport recording is eligible at native1x. With the local unavailable
 // backend it falls back in the same call; legal backing clipping is retained.
 camera::NativeCanvas background;assert(background.reset(24,18));std::vector<uint32_t> backgroundPixels;assert(background.presentRegion(24,18,backgroundPixels,0,0,24,18));
 auto backgroundOwner=std::make_shared<const std::vector<uint32_t>>(backgroundPixels);camera::ViewportCanvas viewport;
 xbrzadapter::gpuPresentation(true);assert(viewport.prepare(background,*backgroundOwner,24,18,0,0,24,18,1,colorprecision::Mode::NativeSteps,1,backgroundOwner));assert(viewport.gpuRecording());
 auto image=std::make_shared<const voxel::Image>(sprite(0xffc03020));lighting::State light;light.intensity=1000;light.tint={{1000,1000,1000}};
 viewport.blit(*image,8,7,1,1,2,light,1,0,image);std::vector<uint32_t> result;assert(viewport.finish(result,true));
 camera::Surface direct;assert(direct.reset(24,18));direct.color=*backgroundOwner;direct.blit(*image,8,7,1,1,2,light,1,0,false);assert(result==direct.color);
 xbrzadapter::gpuPresentation(false);
 // A previously unavailable/disabled GPU must also accept a deferred base:
 // materialize it once before CPU preparation, rather than reject empty pixels.
 int cpuCalls=0;camera::ViewportCanvas cpuPlan;std::vector<uint32_t> emptyPixels;
 assert(cpuPlan.prepare(background,emptyPixels,24,18,0,0,24,18,2,colorprecision::Mode::NativeSteps,1,{},tiles,[&](std::vector<uint32_t>&out){++cpuCalls;out=*backgroundOwner;return true;}));
 assert(cpuCalls==1&&!cpuPlan.gpuRecording());std::vector<uint32_t> cpuResult;assert(cpuPlan.finish(cpuResult,true)&&cpuResult==*backgroundOwner&&cpuCalls==1);
 std::printf("PASS: GPU scene packet ownership, %zu ordered shadow/attachment/SHP replay oracles, raw terrain callback and native1x completion and deferred-base CPU fallback\n",cases);
}
