#include "../../external_reader/source/viewport_canvas.h"
#include "../../external_reader/source/scene_frame_reuse.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace external;
 assert(!xbrzadapter::gpuState().resolve);
 camera::NativeCanvas background;assert(background.reset(64,64));
 auto base=std::make_shared<const std::vector<uint32_t>>(32*32,0xff405060);
 voxel::Image body;body.width=body.height=3;body.pixels.assign(9,0xffc08040);body.flat=true;
 auto owner=std::make_shared<const voxel::Image>(body);
 camera::ViewportCanvas view;lighting::State light;light.intensity=1000;
 xbrzadapter::gpuPresentation(true);
 auto plan=[&](float x,double cameraX=0,bool guides=false){
  assert(view.prepare(background,*base,32,32,cameraX,0,32,32,1,colorprecision::Mode::NativeSteps,1,base,{}, {},1));
  view.motion({},guides);view.blit(*owner,x,10,1,1,20,light,1,0,owner);
  auto p=view.completeGpuPlan();assert(p);return *p;
 };
 camera::FrameReuse<std::vector<uint32_t>> cache;
 auto first=plan(10);assert(!cache.get(first));
 camera::Surface rendered;assert(rendered.reset(32,32));rendered.color=*base;rendered.blit(*owner,10,10,1,1,20,light);
 auto result=std::make_shared<const std::vector<uint32_t>>(rendered.color);cache.completed(first,result);
 for(int frame=1;frame<=1000;++frame){auto next=plan(10);next.motion.frame=frame;next.motion.tick=frame;assert(cache.get(next)==result);}
 assert(!cache.get(plan(11))); // A current position change is never held back.
 assert(plan(1000).draws.empty()); // Offscreen art must not reach upload/raster work.
 voxel::Image extended=body;extended.x=1000;extended.attachments.push_back(body);owner=std::make_shared<const voxel::Image>(extended);
 auto attachmentPlan=plan(10);assert(attachmentPlan.draws.size()==1&&attachmentPlan.draws[0].left==10); // Visible attachment survives its offscreen body.
 owner=std::make_shared<const voxel::Image>(body);
 light.intensity=500;assert(!cache.get(plan(10)));light.intensity=1000;
 assert(!cache.get(plan(10,1)));assert(!cache.get(plan(10,0,true)));
 body.pixels.assign(9,0xff20a040);owner=std::make_shared<const voxel::Image>(body);assert(!cache.get(plan(10)));
 unsigned misses=0;
 auto changed=[&](auto edit){auto f=first;edit(f);assert(!cache.get(f));++misses;};
 changed([](auto&f){f.width++;});changed([](auto&f){f.height++;});
 changed([](auto&f){f.basePixels=std::make_shared<const std::vector<uint32_t>>(*f.basePixels);});
 changed([](auto&f){f.baseDepth=std::make_shared<const std::vector<float>>(*f.baseDepth);});
 changed([](auto&f){f.baseDepthXs=std::make_shared<const std::vector<uint32_t>>(*f.baseDepthXs);});
 changed([](auto&f){f.fogAlpha=std::make_shared<const std::vector<uint8_t>>(32*32,64);});
 changed([](auto&f){f.maskPlan=std::make_shared<const gpushroud::Plan>();});
 changed([](auto&f){f.clipRight--;});changed([](auto&f){f.clipTop++;});
 changed([](auto&f){f.draws[0].depthBase++;});changed([](auto&f){f.draws[0].flat=!f.draws[0].flat;});
 changed([](auto&f){f.draws[0].writeDepth=false;});changed([](auto&f){f.draws[0].shadow=true;});
 changed([](auto&f){f.draws[0].skipZ=true;});changed([](auto&f){f.draws[0].sampling=vulkancompute::Sampling::Bilinear;});
 changed([](auto&f){f.draws[0].reconstructionFactor=2;});changed([](auto&f){f.draws[0].smoothRGB=true;});
 changed([](auto&f){f.draws.clear();});
 auto two=first;two.draws.push_back(two.draws.front());two.draws.back().left++;
 cache.completed(two,result);std::swap(two.draws[0],two.draws[1]);assert(!cache.get(two));
 auto tiled=first;vulkancompute::TileJob tile;tile.pixels=base;tile.width=tile.height=tile.sourceWidth=tile.sourceHeight=32;tiled.baseTiles.push_back(tile);
 cache.completed(tiled,result);assert(cache.get(tiled)==result);tiled.baseTiles[0].sourceX++;assert(!cache.get(tiled));
 cache.completed(first,{});assert(!cache.get(first));cache.completed(first,result);cache.reset();assert(!cache.get(first));
 xbrzadapter::gpuPresentation(false);assert(!xbrzadapter::gpuState().resolve);
 std::printf("PASS 1000 identical current command plans reuse immutable pixels; live position/light/palette/camera/guides and %u output-input mutations, painter order, tiles and reset invalidate; no Vulkan execution\n",misses);
}
