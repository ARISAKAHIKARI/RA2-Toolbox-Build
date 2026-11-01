#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/radar_live_shroud.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace external;
namespace {
constexpr uint32_t Cells=0x100000,Cell=0x200000,Neighbor=0x210000,House=0x300000,Scenario=0x800000;
constexpr size_t Index=5*512+5;
std::vector<uint8_t> memory(0x1000000);
uint32_t failAt=0;size_t failLength=0;bool neighborFieldFail=false,pageOnlyFail=false;
std::vector<uint32_t> prefetched;size_t prefetchSpan=0;
template<class T>void put(uint32_t at,T value){std::memcpy(memory.data()+at,&value,sizeof value);}
bool bytes(uint32_t at,void*out,size_t n){
 if(uint64_t(at)+n>memory.size())return false;
 if(failLength&&at<uint64_t(failAt)+failLength&&uint64_t(at)+n>failAt)return false;
 if(neighborFieldFail&&at==Neighbor+0x24&&n==0x11D)return false;
 if(pageOnlyFail&&at==Cell&&n==4096)return false;
 std::memcpy(out,memory.data()+at,n);return true;
}
struct Read {bool operator()(uint32_t at,void*out,size_t n)const{return bytes(at,out,n);}bool fresh(uint32_t at,void*out,size_t n)const{return bytes(at,out,n);}
 void prefetch(const std::vector<uint32_t>&addresses,size_t span)const{prefetched=addresses;prefetchSpan=span;}
};
void setup(){
 put(radar::Map+0xF4,4);put(radar::Map+0xF8,4);put(radar::Map+0x104,4);put(radar::Map+0x108,4);
 put(radar::Map+0x13C,Cells);put(radar::Map+0x140,4096);put(radar::Map+0x1488,1.f);
 put(radar::Map+0x1490,4);put(radar::Map+0x1498,4);put(radar::Map+0x14A4,8);put(radar::Map+0x14A8,8);
 put(radar::Map+0x1240,8);put(radar::Map+0x1244,8);put(0xA83D4C,House);put(0xA8B230,Scenario);
 put(0xA8ED84,1);put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));
 put(Cells+Index*4,Cell);put(Cells+(Index+1)*4,Neighbor);
 for(auto table:{0xA8ED28u,0xA83D80u,0xA8EC18u}){uint32_t entries=table-0x1000;put(table+4,entries);put(table+8,1);put(table+16,1);put(entries,uint32_t(0x400000));}
 for(auto p:{Cell,Neighbor}){put(p+0x24,int16_t(p==Cell?5:6));put(p+0x26,int16_t(5));put(p+0x12C,uint8_t(0x10));put(p+0x140,uint8_t(1));put(p+0xEC,2);}
}
}
int main(){
 setup();radar::Sampler<Read> sampler(Read{});assert(sampler.step(0,4096,true));
 assert(prefetchSpan==0x148&&prefetched==std::vector<uint32_t>({Cell,Neighbor}));
 auto front=sampler.terrainFrame(1).terrain;auto revision=sampler.revision();assert(front&&front->shade[Index].shroud==-1);
 auto retained=[&](){assert(sampler.terrainFrame(2).terrain==front&&sampler.revision()==revision);assert(front->shade[Index].shroud==-1);};
 unsigned rejected=0;
 for(auto failure:std::vector<std::pair<uint32_t,size_t>>{{Cell,0x148},{Cells,4096*4},{0x7F4194,256},{radar::Map+0x1240,4},{radar::Map+0x1490,4},{radar::Map+0x1498,4},
   {0xA8ED2C,4},{0xA83D90,4},{0xA8EC20,4},{0xA8DD28,4},{0xA82D80,4},{0xA8DC18,4},{0xB0F4EC,4},{0x4002F0,4},{0x4002A9,1}}){
  failAt=failure.first;failLength=failure.second;assert(!sampler.step(0,4096,true));retained();++rejected;
  failLength=0;assert(sampler.step(0,4096,true));front=sampler.terrainFrame(3).terrain;revision=sampler.revision();
 }
 // A cell naming a material outside a verified catalogue is incomplete,
 // whereas a verified absent material (-1) is allowed to remove old art.
 for(uint32_t field:{0x38u,0x44u,0x48u}){put(Cell+field,2);assert(!sampler.step(0,4096,true));retained();++rejected;put(Cell+field,0);}
 put(Cell+0x44,-1);assert(sampler.step(0,4096,true));assert(!sampler.terrainFrame(3).terrain->art[Index].overlayType);
 put(Cell+0x44,0);assert(sampler.step(0,4096,true));front=sampler.terrainFrame(3).terrain;revision=sampler.revision();assert(front->art[Index].overlayType==0x400000);
 put(Cell+0x24,int16_t(4));assert(!sampler.step(0,4096,true));retained();++rejected;put(Cell+0x24,int16_t(5));
 put(Cells+Index*4,uint32_t(4));assert(!sampler.step(0,4096,true));retained();++rejected;put(Cells+Index*4,Cell);
 // Only a selector-neighbor read fails: its main Cell read still succeeds.
 // A successful later read must not turn this incomplete batch into a pass.
 put(Cell+0x12C,uint8_t(0x08));neighborFieldFail=true;
 assert(!sampler.step(0,4096,true));retained();++rejected;neighborFieldFail=false;put(Cell+0x12C,uint8_t(0x10));
 // Failure after two successful bounded chunks must not publish half a bank.
 assert(sampler.step(0,1024,true));assert(prefetched.empty());assert(sampler.step(0,1024,true));assert(prefetched.empty());retained();
 failAt=Cell;failLength=0x148;assert(!sampler.step(0,1024,true));retained();++rejected;
 assert(prefetched==std::vector<uint32_t>({Cell,Neighbor}));
 radar::Sampler<Read> first(Read{});assert(!first.step(0,4096,true)&&!first.hasTerrain());
 failLength=0;for(int i=0;i<4;++i)assert(sampler.step(0,1024,true));
 assert(sampler.revision()==revision+1&&sampler.terrainFrame(4).terrain!=front);
 // Null physical entries are legitimate holes, not transient read failures.
 put(Cells+(Index+1)*4,uint32_t(0));assert(sampler.step(0,4096,true));put(Cells+(Index+1)*4,Neighbor);
 auto geo=sampler.terrainFrame(5).terrain->geo;
 radar::shroudmask::Live live;radar::shroudmask::Snapshot out;
 assert(live.capture(Read{},geo,0,0,240,120,out)&&!out.cells.empty());
 failAt=Cell;failLength=0x148;assert(!live.capture(Read{},geo,0,0,240,120,out)&&out.cells.empty());failLength=0;
 assert(live.capture(Read{},geo,0,0,240,120,out));
 pageOnlyFail=true;assert(live.capture(Read{},geo,0,0,240,120,out));pageOnlyFail=false; // exact-field fallback succeeds
 put(Cell+0x24,int16_t(4));assert(!live.capture(Read{},geo,0,0,240,120,out));put(Cell+0x24,int16_t(5));
 put(Cell+0x12C,uint8_t(0x08));failAt=Neighbor;failLength=0x148;
 assert(!live.capture(Read{},geo,0,0,240,120,out));failLength=0;put(Cell+0x12C,uint8_t(0x10));
 put(Cells+(Index+1)*4,uint32_t(0));assert(live.capture(Read{},geo,0,0,240,120,out));
 // A verified world change still drops the previous bank immediately.
 put(0xA83D4C,House+0x1000);assert(sampler.step(0,1024,true)&&!sampler.hasTerrain());
 assert(!live.capture(Read{},geo,0,0,240,120,out));
 for(int i=0;i<3;++i)assert(sampler.step(0,1024,true));assert(sampler.terrainFrame(6).terrain->geo.house==House+0x1000);
 std::printf("PASS: %u incomplete terrain batches rejected; front retained/recovered; live masks reject missing reads; null cells and page fallback remain valid; world changes invalidate\n",rejected);
}
