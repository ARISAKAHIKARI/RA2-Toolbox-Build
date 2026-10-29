#include "../source/render_presentation.h"
#include "../../shared/source/latest_worker.h"
#include "../../shared/source/overlay_group.h"
#include <cassert>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
struct Request {
 uint64_t generation=1;bool materialRefresh=true;int detail=1,colorPrecision=1;uint32_t pid=77,match=8,tick=1000;
 int width=4,height=3;uint32_t reveal=0;int paletteRange=1,pixelScaleMode=3;
 external::mainview::View view{0x10000,0x20000,0x30000,50,100,200,800,600,0,0,640,600};
 overlaycomposition::Context context{9,101,77,8,4,3,100,200};
 bool fail=false;int sequence=0;bool gpu=false,neural=false;
};
struct Frame {Request request;std::shared_ptr<const std::vector<uint32_t>> image;uint32_t sampledTick=0,completedTick=0;};
template<class F>void eventually(F fn){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!fn()){assert(std::chrono::steady_clock::now()<end);std::this_thread::yield();}}
int main(){
 using namespace renderpresentation;
 Request request;Frame frame{request,std::make_shared<const std::vector<uint32_t>>(12,0xff123456),1000,1450};
 // A 450ms render is usable immediately after completion, then pending and
 // transiently failed work leave its pixels unchanged until the bounded lease.
 assert(usable(frame,request,1450));assert(usable(frame,request,2100));
 assert(!usable(frame,request,2950));
 frame.completedTick=3400;assert(!usable(frame,request,3500)); // old sample cannot be laundered by a late completion
 frame.sampledTick=0xfffffff0u;frame.completedTick=0xfffffff8u;assert(usable(frame,request,50));
 frame={request,std::make_shared<const std::vector<uint32_t>>(12,0xff123456),1000,1450};
 auto changed=request;changed.view.cameraX++;assert(!usable(frame,changed,1450));
 changed=request;changed.view.scenario++;assert(!usable(frame,changed,1450));
 changed=request;changed.view.house++;assert(!usable(frame,changed,1450));
 changed=request;changed.match++;assert(!usable(frame,changed,1450));
 changed=request;changed.context.token++;assert(!usable(frame,changed,1450));
 changed=request;changed.generation++;assert(!usable(frame,changed,1450));
 changed=request;changed.reveal++;assert(!usable(frame,changed,1450));
 changed=request;changed.colorPrecision=0;assert(!usable(frame,changed,1450));
 changed=request;changed.gpu=true;assert(!usable(frame,changed,1450));
 changed=request;changed.neural=true;assert(!usable(frame,changed,1450));
 changed=request;changed.width++;assert(!usable(frame,changed,1450));
 assert(sampleAdvance(100,100,100)&&sampleAdvance(100,101,103));
 assert(!sampleAdvance(100,100,103)&&!sampleAdvance(100,99,100)&&!sampleAdvance(100,101,100));
 ViewLease lease;assert(!lease.retain(101,77,8,4,3,1000));
 lease.observe(101,77,8,4,3,request.view,1000);
 assert(lease.retain(101,77,8,4,3,1119)&&!lease.retain(101,77,8,4,3,1120));
 assert(!lease.retain(101,77,9,4,3,1010)&&!lease.retain(101,77,8,5,3,1010));
 lease.reset();assert(!lease.retain(101,77,8,4,3,1010));
 std::atomic<int> attempted{0};
 asyncwork::Latest<Request,Frame> worker([&](const Request&r)->std::shared_ptr<const Frame>{
  attempted=r.sequence;if(r.fail)return {};
  auto out=std::make_shared<Frame>();out->request=r;out->sampledTick=r.tick;out->completedTick=r.tick+450;
  out->image=std::make_shared<const std::vector<uint32_t>>(12,0xff000000u|unsigned(r.sequence));return out;
 });
 std::shared_ptr<const Frame> last;
 for(int n=1;n<=30;++n){
  request.sequence=n;request.tick=1000+uint32_t(n)*100;request.fail=n%3!=1;
  worker.submit(request);eventually([&]{return attempted.load()==n;});
  if(!request.fail){eventually([&]{auto f=worker.get();return f&&f->request.sequence==n;});last=worker.get();}
  auto f=worker.get();assert(f&&f==last&&usable(*f,request,request.tick+450));
  std::vector<overlaycomposition::Surface> layers{{3,110,request.context,{0,0,4,3},true,255,f->image}};
  assert(overlaycomposition::groupValid({3,4,5},layers,request.context));
  std::vector<uint32_t> pixels;assert(overlaycomposition::compose(request.context,layers,pixels));
  assert(pixels==*last->image); // no native/transparent frame on pending/failure
 }
 worker.invalidate();assert(!worker.get());worker.shutdown();
 std::puts("PASS: bounded completed-frame continuity, slow-render sample age, viewport/config invalidation, read grace, and 30 async success/failure publications");
}
