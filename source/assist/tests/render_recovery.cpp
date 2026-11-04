#include "../source/render_presentation.h"
#include "../../external_reader/source/gpu_scene_record.h"
#include "../../external_reader/source/native_display_order.h"
#include <cassert>
#include <cstdio>

struct Request {
 uint64_t generation=1;bool materialRefresh=false;int detail=0,colorPrecision=0;
 uint32_t pid=77,match=8;int width=640,height=480;uint32_t reveal=0;
 int paletteRange=0,pixelScaleMode=1;bool gpu=false;
 external::mainview::View view{0x10000,0x20000,0x30000,50,100,200,800,600,0,0,640,600};
 overlaycomposition::Context context{9,101,77,8,640,480,100,200};
};
struct Image {int width=640,height=480;};
struct Frame {Request request;std::shared_ptr<const Image>image;uint32_t sampledTick=1000,completedTick=1250;};

int main(){
 using namespace renderpresentation;using namespace external;
 Request request;auto completed=std::make_shared<Frame>();completed->request=request;completed->image=std::make_shared<const Image>();
 GroupLease<Request,Frame> lease;
 auto pixels=std::make_shared<const std::vector<uint32_t>>(640*480,0xff123456);
 auto holes=std::make_shared<const std::vector<overlaycomposition::Rect>>(std::vector<overlaycomposition::Rect>{{2,2,3,1}});
 std::vector<overlaycomposition::Surface>layers{{3,5,request.context,{0,0,640,480},true,255,pixels,{},holes},
  {4,200,request.context,{0,0,640,480},true,255,pixels}};
 lease.publish(request,completed,layers,1250);
 // Simulate repeated UI polls while the next full render takes 250 ms.
 // Every poll between completions must retain the same complete picture.
 for(uint32_t now=1250;now<1750;++now){assert(completeUsable(*completed,request,now));assert(lease.retainWorld(request,now));}
 auto panned=request;panned.view.cameraX+=200;panned.context.cameraX+=200;
 assert(lease.retainWorld(panned,1500));assert(lease.context().cameraX==100);
 auto world=worldOnly(lease.layers(),3);assert(world.size()==1&&!world[0].cutouts);
 assert(world[0].pixels==pixels&&world[0].context.cameraX==100&&world[0].rect.x==0);
 assert(layers[0].cutouts==holes); // immutable original transaction survives
 auto changed=panned;++changed.match;assert(!lease.retainWorld(changed,1500));
 changed=panned;++changed.generation;assert(!lease.retainWorld(changed,1500));
 changed=panned;changed.gpu=true;assert(!lease.retainWorld(changed,1500));
 changed=panned;++changed.context.token;assert(!lease.retainWorld(changed,1500));
 assert(!lease.retainWorld(panned,2750)); // retries cannot renew completion
 completed->sampledTick=0xfffffff0u;completed->completedTick=0xfffffff8u;
 assert(lease.retainWorld(request,300));assert(!lease.retainWorld(request,1600));
 lease.reset();assert(!lease.retainWorld(request,300));

 // Full-frame replay above HF28's row-parallel threshold. Independent color
 // and native-Z expectations also check order, attachment and untouched alpha.
 constexpr int W=640,H=480;
 auto base=std::make_shared<const std::vector<uint32_t>>(W*H,0xff102030);
 auto depth=std::make_shared<const std::vector<float>>(W*H,-10000.f);
 camera::GpuSceneRecord record;record.begin(W,H,base,depth,colorprecision::Mode::NativeSteps);record.nativeView(1,1,true);
 voxel::Image red;red.width=80;red.height=400;red.flat=true;red.nativeDepth.enabled=true;
 red.nativeDepth.gradient=nativesurface::Gradient::Flat;red.pixels.assign(80*400,0xffff0000);
 voxel::Image green=red;green.width=40;green.height=30;green.y=150;green.pixels.assign(1200,0xff00ff00);
 red.attachments.push_back(green);auto owner=std::make_shared<const voxel::Image>(red);
 lighting::State light;light.intensity=1000;camera::Material zeroAdjust;zeroAdjust.nativeZAdjust=0;
 assert(record.append(*owner,owner,100,20,1,1,100,light,1,0,false,{},zeroAdjust));
 camera::Surface surface;assert(record.replay(surface));
 for(int y=0;y<H;++y)for(int x=0;x<W;++x){size_t n=size_t(y)*W+x;
  bool body=x>=100&&x<180&&y>=20&&y<420;
  // Equal native depth rejects a later same-plane attachment.
  assert(surface.color[n]==(body?0xffff0000u:0xff102030u));
  if(surface.nativeZ[n]!=(body?uint16_t(32768-y):uint16_t(65535))){std::fprintf(stderr,"Z mismatch x=%d y=%d actual=%u expected=%u\n",x,y,unsigned(surface.nativeZ[n]),unsigned(body?32768-y:65535));std::abort();}
  assert(surface.alphaAt(n)==127);
 }
 // Native waterline clip at 0.5x was one row different on the GPU path.
 for(float scale:{.5f,1.f,1.5f,2.f}){
  record.begin(W,H,base,depth,colorprecision::Mode::NativeSteps);record.nativeView(scale,scale,true);
  camera::Material material;material.clipBottom=100;material.nativeZAdjust=0;
  assert(record.append(*owner,owner,20,0,scale,scale,100,light,1,0,false,{},material));
  const auto&draw=record.frame().draws.front();auto expected=red.nativeDepth;
  auto walk=nativesurface::walk(expected,int(std::floor(draw.top/scale)),400,100,0,false);
  assert(draw.nativeWalk==walk);assert(nativesurface::clippedViewHeight(0,scale,100,1000)==100);
 }
 assert(!gpupresent::DirectPresentationEnabled&&!gpupresent::available());
 assert(!xbrzadapter::gpuState().resolve); // CPU test did not load a GPU device
 struct Marker {uint32_t object=0x30000,id=88;int layer=99,displayOrder=99;};
 for(bool changeDuringIdentity:{false,true}){
  int frame=50;std::vector<Marker>markers(1);
  auto read=[&](uint32_t address,void*out,size_t bytes){
   if(bytes!=4)return false;uint32_t value=0;
   if(address==0xA8ED84)value=uint32_t(frame);
   else if(address==0x20000)value=0x30000;
   else if(address==0x30010){value=88;if(changeDuringIdentity)++frame;}
   else if(address>=0x8A0360&&address<0x8A0360+5*24){
    auto offset=address-0x8A0360;
    if(offset/24==0)value=offset%24==4?0x20000:offset%24==8||offset%24==16?1:0;
   }else return false;
   std::memcpy(out,&value,4);return true;
  };
  assert(nativedisplay::capture(read,markers,50)==!changeDuringIdentity);
  assert(markers[0].layer==(changeDuringIdentity?99:0));
  assert(markers[0].displayOrder==(changeDuringIdentity?99:0));
 }
 std::puts("PASS: 500 slow-frame polls; camera/world/config/expiry/wrap continuity; full-size ARGB/u16Z/A ordered replay; four waterline scales; final Display fence; CPU-only execution");
}
