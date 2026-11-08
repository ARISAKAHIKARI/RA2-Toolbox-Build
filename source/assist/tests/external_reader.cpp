#include "../../external_reader/source/cooperative_reader.h"
#include "../../external_reader/source/tactical_sampler.h"
#include "../../shared/source/world_frame.h"
#include <cassert>
#include <vector>
#include <cstring>
#include <iostream>
struct Memory{std::vector<unsigned char> bytes=std::vector<unsigned char>(0xC00000);uint32_t reads=0,worldScans=0;template<class T>void put(uint32_t p,T x){memcpy(bytes.data()+p,&x,sizeof x);}void vec(uint32_t p,uint32_t a,std::initializer_list<uint32_t> items){put(p+4,a);put<int>(p+8,items.size());put<int>(p+16,items.size());for(auto x:items){put(a,x);a+=4;}}bool read(uint32_t a,void*out,size_t n){++reads;if(a==0xA8EC78+4)++worldScans;if(a<0x10000||uint64_t(a)+n>bytes.size())return false;memcpy(out,bytes.data()+a,n);return true;}};
struct Read{Memory*m;bool operator()(uint32_t a,void*out,size_t n)const{return m->read(a,out,n);}};
struct SealedRead {
 Memory*memory;worldframe::Snapshot*source;bool*complete;
 bool operator()(uint32_t at,void*out,size_t n)const{bool ok=source->read(at,out,n);if(!ok)*complete=false;return ok;}
 bool live(uint32_t at,void*out,size_t n)const{return memory->read(at,out,n);}
};
int main(){using namespace assist;Memory m;Read read{&m};constexpr uint32_t house=0x400000,tactical=0x450000,type=0x300000,unit=0x200000,temporal=0x500000;
 m.put<uint8_t>(0xA8E9A0,1);m.put<uint8_t>(0xA8E378,1);m.put<int>(0xA8ED84,100);m.put(0xA83D4C,house);m.put(0x887324,tactical);m.put<int>(0x8A00A4,1920);m.put<int>(0x8A00A8,1080);m.put<int>(tactical+0xB0,-320);m.put<int>(tactical+0xB4,800);m.put<double>(0xB0CD48,.0866025403784);m.vec(0xA80228,0x110000,{house});m.vec(0xA8EC78,0x110100,{unit});m.vec(0xA8B218,0x110200,{type});m.vec(0xA8ECB8,0x110300,{unit});
 m.put<int>(type+0x634,1);m.put<uint32_t>(type+0x6CC,1);m.put<int>(type+0x5E0,5);m.put(unit,uint32_t(0x7F5C70));m.put<uint32_t>(unit+0x10,1);m.put(unit+0x6C4,type);m.put(unit+0x21C,house);m.put<int>(unit+0x6C,100);for(int off:{0x74,0x90,0x83})m.put<uint8_t>(unit+off,1);m.put<int>(unit+0x9C,10240);m.put<int>(unit+0xA0,10240);m.put<int>(unit+0x18C,50);m.put<int>(unit+0x194,150);m.put<uint8_t>(house+22265,80);m.put<uint8_t>(house+22266,180);m.put<uint8_t>(house+22267,240);
 m.put(unit+0x278,temporal);m.put(temporal+0x28,unit);m.put<int>(temporal+0x48,1000);m.put<int>(temporal+0x4C,20);m.put(temporal+0x44,temporal);
 external::TacticalSampler<Read> sampler(read);auto s=sampler.sample(100,Capacity|Iron|Frozen);assert(s.status==1&&s.renderStatus==1&&s.typeCount==1&&s.labels==1&&s.label[0].ownerColor==0xFF50B4F0&&s.label[0].ironFrames==100&&s.label[0].frozenFrames==50);
 m.reads=0;sampler.sample(101,Capacity|Iron|Frozen);auto cached=m.reads;m.reads=0;m.put<int>(0xA8ED84,101);sampler.sample(102,Capacity|Iron|Frozen);assert(m.reads>cached);
 const auto& prior=sampler.sample(102,Capacity|Iron|Frozen);m.put<int>(tactical+0xB0,-300);sampler.refreshCamera(108);assert(prior.sampledTick==102);assert(sampler.sample(133,Capacity|Iron|Frozen).label[0].x==300);
 // Display suppression must not create a new match or destroy read data.
 const auto epoch=sampler.sample(140,Capacity|Iron|Frozen).match;
 for(uint32_t address:{0xA8EDA0u,0xA8ED8Cu}){
  m.put<int>(address,1);auto hidden=sampler.sample(142,Capacity|Iron|Frozen);
  assert(hidden.status==1&&hidden.renderStatus==0&&hidden.match==epoch);
  m.put<int>(address,0);auto shown=sampler.sample(144,Capacity|Iron|Frozen);assert(shown.renderStatus==1&&shown.match==epoch);
 }
 m.put<uint8_t>(0xA8E378,0);assert(sampler.sample(146,Capacity|Iron|Frozen).renderStatus==0);
 m.put<uint8_t>(0xA8E378,1);assert(sampler.sample(148,Capacity|Iron|Frozen).renderStatus==1);
 // Paused frame is NOT a menu. Same-frame moves and boarding must be fresh.
 auto before=sampler.sample(150,Capacity|Iron|Frozen);m.put<int>(unit+0x9C,10240+256);
 auto moved=sampler.sample(152,Capacity|Iron|Frozen);assert(moved.frame==before.frame&&moved.label[0].x==before.label[0].x+30&&moved.renderStatus==1);
 constexpr uint32_t rider=0x210000,riderType=0x310000;
 m.put(unit+0x118,rider);m.put(rider,uint32_t(0x7EB058));m.put(rider+0x6C0,riderType);m.put<double>(riderType+0x380,2.);
 auto boarded=sampler.sample(154,Capacity|Iron|Frozen);assert(boarded.label[0].used==2&&boarded.used==2);
 m.put<uint32_t>(unit+0x118,0);auto unloaded=sampler.sample(156,Capacity|Iron|Frozen);assert(unloaded.used==0&&unloaded.label[0].used==0);
 // Continuous spectator-style panning keeps display coordinates fresh at
 // 500 Hz without enumerating every world object at 500 Hz. Leaving the
 // viewport does not cause a repeated full scan; new visible candidates are
 // discovered at the next logic frame or within the 16 ms camera interval.
 m.worldScans=0;
 for(int i=0;i<1000;++i){m.put<int>(tactical+0xB0,-300+i*3);auto pan=sampler.sample(200+2*i,Capacity|Iron|Frozen);if(pan.labels)assert(pan.label[0].x==330-i*3);}
 assert(m.worldScans<=126&&m.worldScans>0);
 m.put<int>(tactical+0xB0,-300);sampler.sample(2300,Capacity|Iron|Frozen);
 std::cout<<"PASS: 1000 camera samples retain high-rate positions with at most 126 full world enumerations, including viewport exit\n";
 // Recycling/removal inside one frame invalidates cached label identity.
 m.put<uint32_t>(unit+0x10,9);assert(sampler.sample(2302,Capacity|Iron|Frozen).label[0].id==9);
 m.put<int>(unit+0x6C,0);assert(sampler.sample(2304,Capacity|Iron|Frozen).labels==0);
 auto failed=external::display::inspect([](uint32_t,void*,size_t){return false;});assert(!failed.readable&&!failed.visible());
 std::cout<<"PASS: menu enter/exit preserves epoch, paused battlefield visible, same-frame position/boarding/unloading, recycled/dead object and unreadable display state\n";
 // Recover empty/partial catalogues while the same process and House remain.
 constexpr uint32_t secondType=0x320000;std::memcpy(m.bytes.data()+type+0x24,"HTNK",5);std::memcpy(m.bytes.data()+secondType+0x24,"FV",3);
 m.put<int>(secondType+0x634,1);m.put<uint32_t>(secondType+0x6CC,1);
 auto oldEpoch=sampler.sample(3000,Capacity).match;
 m.vec(0xA8B218,0x110200,{type,secondType});m.put<int>(0xA8ED84,102);
 assert(sampler.sample(3600,Capacity).typeCount==2&&sampler.sample(3602,Capacity).match==oldEpoch);
 m.put<uint32_t>(secondType+0x6CC,0);assert(sampler.sample(4200,Capacity).typeCount==1);
 m.put<uint32_t>(secondType+0x6CC,1);assert(sampler.sample(4800,Capacity).typeCount==2);
 m.vec(0xA8B218,0x110200,{});assert(sampler.sample(5400,Capacity).typeCount==0);
 m.vec(0xA8B218,0x110200,{type});assert(sampler.sample(5500,Capacity).typeCount==1);
 std::cout<<"PASS: delayed type-vector growth, metadata completion and empty-to-ready recovery in one process/epoch\n";
 int x,y;external::Projection projection;projection.width=1920;projection.height=1080;assert(!external::project(projection,INT32_MIN,0,0,x,y));
 std::cout<<"PASS: actual-layout external snapshot, owner colors, iron/frozen timers with cycle guard, unchanged-frame cache, camera shift and invalid coordinates\n";
 // Production TacticalSampler with a real sealed read set. World movement
 // remains fixed within the logic frame; selection and camera are live UI.
 m.put<int>(unit+0x6C,100);m.put<uint8_t>(unit+0x83,1);m.vec(0xA8ECB8,0x110300,{unit});
 worldframe::Snapshot raw;worldframe::Key key{};key.match=99;key.frame=102;key.serial=1;key.tick=6000;
 auto acquire=[&](bool omitActor){raw.reset(key);for(uint32_t p=0x10000;p<m.bytes.size();p+=4096)if(!omitActor||p!=(unit&~4095u))assert(raw.prefetch(read,p,4096));assert(raw.seal());};
 acquire(false);bool complete=true;external::TacticalSampler<SealedRead> sealed({&m,&raw,&complete});sealed.worldEpoch(key.match);
 auto captured=sealed.sample(key.tick,Capacity|Iron|Frozen);assert(complete&&captured.labels==1&&captured.match==99);sealed.commitSample();
 m.put<int>(unit+0x9C,10240+512);m.put<uint8_t>(unit+0x83,0);m.vec(0xA8ECB8,0x110300,{});
 auto changedUI=sealed.sample(key.tick,Capacity|Iron|Frozen);assert(complete&&changedUI.labels==1&&changedUI.label[0].x==captured.label[0].x&&changedUI.selected==0);
 m.put<int>(tactical+0xB0,-200);sealed.refreshCamera(9000);assert(sealed.state().sampledTick==key.tick&&sealed.state().label[0].x==changedUI.label[0].x-100);
 auto stable=sealed.state();sealed.commitSample();
 // Rejecting an incomplete next frame preserves the stable publication even
 // on the subsequent camera-only fast path (no partial sampler state leaks).
 ++key.serial;++key.frame;key.tick=6016;m.put<int>(0xA8ED84,key.frame);acquire(true);complete=true;
 sealed.sample(key.tick,Capacity|Iron|Frozen);assert(!complete);sealed.invalidateSample(&stable);
 m.put<int>(tactical+0xB0,-180);sealed.refreshCamera(9100);assert(sealed.state().frame==stable.frame&&sealed.state().sampledTick==stable.sampledTick&&sealed.state().label[0].id==stable.label[0].id&&sealed.state().label[0].x==stable.label[0].x-20);
 acquire(false);complete=true;auto recovered=sealed.sample(key.tick,Capacity|Iron|Frozen);assert(complete&&recovered.frame==key.frame&&recovered.label[0].x!=stable.label[0].x);sealed.commitSample();
 std::cout<<"PASS: sealed world, independent paused selection/camera, original acquisition tick, transactional failure/camera retry and next-frame recovery\n";
}

