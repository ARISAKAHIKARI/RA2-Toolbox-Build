#include "../source/formation.h"
#include "../../external_reader/source/projection.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <vector>
using namespace assist;
static uint32_t tick=1000;static uint32_t GetTickCount(){return tick;}
#include "../../shared/source/world_frame.h"
#include "../source/special_unit_overlay_engine.h"
struct Memory {
 std::vector<uint8_t>bytes=std::vector<uint8_t>(0xC00000);
 template<class T>void put(uint32_t p,T v){std::memcpy(bytes.data()+p,&v,sizeof v);}
 bool read(uint32_t p,void*out,size_t n){if(p<0x10000||uint64_t(p)+n>bytes.size())return false;std::memcpy(out,bytes.data()+p,n);return true;}
};
struct Reader {worldframe::Snapshot frame;bool complete=true;
 bool readFresh(uint32_t p,void*out,size_t n){const bool ok=frame.read(p,out,n);if(!ok)complete=false;return ok;}
 bool worldFrameComplete()const{return complete;}
};
struct Assets {
 struct ActorArt {external::voxel::Model model;uint32_t type=0;};
 unsigned captures=0,renders=0;std::vector<uint32_t>capturedTypes;std::vector<external::visual::Pose>poses;
 std::map<std::tuple<uint32_t,int,int>,std::shared_ptr<const external::voxel::Image>> images;
 void identity(uint32_t,uint32_t){}uint64_t revision(){return 1;}
 bool captureActor(uint32_t,uint32_t t,uint32_t,ActorArt&a){++captures;a.type=t;capturedTypes.push_back(t);return true;}
 bool renderSharedActor(const ActorArt&a,const external::visual::Pose&p,std::shared_ptr<const external::voxel::Image>&out){
  ++renders;poses.push_back(p);auto key=std::make_tuple(a.type,p.body,p.frame);auto&image=images[key];
  if(!image){auto next=std::make_shared<external::voxel::Image>();next->width=next->height=1;next->pixels={0xff804000u|p.body};image=next;}out=image;return true;
 }
};
int main(){
 Memory memory;Reader reader;Assets assets;specialoverlay::Renderer<Reader,Assets>render(reader,assets);render.paint.process=7;render.paint.epoch=1;render.paint.decodeBudgetMs=10000;
 constexpr uint32_t a=0x200000,b=0x210000,t=0x300000,spy=0x320000,owner=0x400000;
 auto setup=[&](uint32_t p,uint32_t type,int kind,int cloak,int angle){
  memory.put(p,kind==1?0x7EB058u:0x7F5C70u);memory.put(p+0x10,p+1);memory.put(p+0x21C,owner);memory.put(p+(kind==1?0x6C0:0x6C4),type);
  memory.put(p+0x6C,100);memory.put(p+0x74,uint8_t(1));memory.put(p+0x90,uint8_t(1));memory.put(p+0x220,cloak);
  memory.put(p+0x9C,1024);memory.put(p+0xA0,512);memory.put(p+0x388,uint16_t(angle));
 };
 setup(a,t,2,2,16384);setup(b,spy,1,0,32768);memory.put(spy+0xEBE,uint8_t(1));
 memory.put(b+0x1D8,uint8_t(1));memory.put(b+0x518,t);memory.put(b+0x51C,0xDEADBEEFu);
 memory.put(spy+0xE3C,0x500000u);memory.put(0x500004,1);memory.put(0x500008,1);
 for(unsigned i=0;i<32;++i)memory.put(0x7EAEFC+i*4,int(i/4));
 std::vector<external::enemies::Marker>targets(2);
 targets[0].object=a;targets[0].id=a+1;targets[0].type=t;targets[0].owner=owner;targets[0].kind=2;
 targets[1].object=b;targets[1].id=b+1;targets[1].type=spy;targets[1].owner=owner;targets[1].kind=1;
 auto seal=[&](uint32_t omit=0){++tick;worldframe::Key key{};key.frame=tick;reader.frame.reset(key);auto read=[&](uint32_t p,void*out,size_t n){return memory.read(p,out,n);};
  for(uint32_t p=0x10000;p<memory.bytes.size();p+=4096)if(p!=omit)assert(reader.frame.prefetch(read,p,4096));assert(reader.frame.seal());reader.complete=true;};
 external::Projection projection{0,0,800,600};
 seal();render.update(targets,projection,tick);assert(reader.complete&&render.paint.visuals.size()==2&&render.identities.size()==2);
 assert(assets.capturedTypes[0]==t&&assets.capturedTypes[1]==spy);assert(assets.poses[0].body==8&&assets.poses[1].body==16);
 auto image=render.compose(false,1);assert(image);bool opaque=false;for(auto p:image->pixels)opaque|=(p>>24)==255;assert(opaque);
 assert(render.compose(false,1)==image);auto ghost=render.paint.compose(false,1);unsigned alpha=0;for(auto p:ghost->pixels)alpha=std::max(alpha,p>>24);assert(alpha==124);assert(render.compose(false,1)!=ghost);
 std::puts("PASS true unit Type/owner, independent headings, opaque additive pixels, native formation opacity unchanged");
 render.paint.decodeBudgetMs=-1;seal();render.update(targets,projection,tick);assert(reader.complete&&render.identities.size()==2&&render.paint.missing==1);
 tick+=151;seal();render.update(targets,projection,tick);assert(render.identities.size()==1);
 render.paint.decodeBudgetMs=10000;seal();render.update(targets,projection,tick);assert(render.identities.size()==2);
 std::puts("PASS bounded decode reuses recent same-identity pose without alternating visibility or extending expiry");
 memory.put(a+0x9C,1536);seal();const int x=render.paint.visuals[0].x;render.update(targets,projection,tick);assert(reader.complete);bool moved=false;for(const auto&v:render.paint.visuals)moved|=v.x==x+60;assert(moved);
 memory.put(a+0x6C,0);seal();render.update(targets,projection,tick);assert(render.identities.size()==1&&render.identities[0].object==b);
 memory.put(a+0x6C,100);memory.put(a+0x11C,0x220000u);seal();render.update(targets,projection,tick);assert(render.identities.size()==1);
 memory.put(a+0x11C,0u);memory.put(a+0x10,999u);seal();render.update(targets,projection,tick);assert(render.identities.size()==1);
 memory.put(a+0x10,a+1);memory.put(t+0xC9A,uint8_t(1));seal();render.update(targets,projection,tick);assert(render.identities.size()==1);
 memory.put(t+0xC9A,uint8_t(0));memory.put(a+0x220,0);std::memcpy(memory.bytes.data()+t+0x24,"MGTK",5);seal();render.update(targets,projection,tick);assert(render.identities.size()==2);
 std::puts("PASS current position, death, passenger, recycled identity, permanent Invisible, undisguised Mirage");
 memory.put(a+0x9C,100000);seal();render.update(targets,projection,tick);assert(render.identities.size()==1);memory.put(a+0x9C,1536);
 seal(t&~4095u);render.update(targets,projection,tick);assert(!reader.complete);
 seal();render.update(targets,projection,tick);assert(reader.complete&&render.identities.size()==2);
 auto current=std::make_unique<external::enemies::Frame>();current->count=2;std::copy(targets.begin(),targets.end(),current->items);
 assert(specialoverlay::stillPresent(render.identities,*current));++current->items[0].owner;assert(!specialoverlay::stillPresent(render.identities,*current));--current->items[0].owner;++current->items[0].type;assert(!specialoverlay::stillPresent(render.identities,*current));
 assert(specialoverlay::enabled(true,true,1)&&!specialoverlay::enabled(false,true,1)&&!specialoverlay::enabled(true,false,1)&&!specialoverlay::enabled(true,true,0));
 std::puts("PASS viewport culling, real missing-page rejection/recovery, removal/type/owner validation and exclusive mode admission");
 // Clearing does not mutate already published immutable image bytes.
 auto saved=render.compose(false,1);auto pixels=saved->pixels;render.reset();assert(render.paint.visuals.empty()&&saved->pixels==pixels);
}
