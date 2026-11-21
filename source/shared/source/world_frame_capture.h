#pragma once
#include "world_frame.h"
#include "world_frame_recovery.h"
#include "world_frame_page_sort.h"
#include <limits>

namespace worldframe {
struct SceneGeometry {
 std::array<int,6> bounds{};int width=0,height=0;bool readable=false;
 bool same(const SceneGeometry&other)const{
  return readable&&other.readable&&bounds==other.bounds&&width==other.width&&height==other.height;
 }
};
template<class Read>SceneGeometry sceneGeometry(Read read){
 SceneGeometry geometry;
 geometry.readable=read(0x87F7E8+0xF4,geometry.bounds.data(),sizeof geometry.bounds)&&
  read(0x87F7E8+0x1240,&geometry.width,sizeof geometry.width)&&
  read(0x87F7E8+0x1244,&geometry.height,sizeof geometry.height)&&geometry.width>0&&geometry.height>0;
 return geometry;
}
// Capture membership is temporary, but the game-thread allocator is shared
// with the engine. Reusing flat buckets avoids allocating and freeing one
// unordered_set node per object on every logic frame. Generations clear only
// membership, never preserve observations or skip the next frame's live read.
class CaptureSet {
 struct Entry {uint32_t address,generation;};
 Entry*entries_=nullptr;size_t size_=0,capacity_=0;uint32_t generation_=1;bool failed_=false;PageAllocator allocator_;
 static size_t hash(uint32_t address){address^=address>>16;address*=0x7feb352du;address^=address>>15;address*=0x846ca68bu;return address^(address>>16);}
 size_t slot(uint32_t address)const{size_t i=hash(address)&(capacity_-1);while(entries_[i].generation==generation_&&entries_[i].address!=address)i=(i+1)&(capacity_-1);return i;}
 bool grow(){
  if(capacity_>(std::numeric_limits<size_t>::max)()/2/sizeof(Entry)){failed_=true;return false;}
  const size_t next=capacity_?capacity_*2:32;auto*replacement=static_cast<Entry*>(allocator_.allocate(next*sizeof(Entry),allocator_.context));
  if(!replacement){failed_=true;return false;}
  for(size_t i=0;i<next;++i)::new(static_cast<void*>(replacement+i))Entry{};
  for(size_t i=0;i<capacity_;++i)if(entries_[i].generation==generation_){size_t at=hash(entries_[i].address)&(next-1);while(replacement[at].generation)at=(at+1)&(next-1);replacement[at]={entries_[i].address,generation_};}
  if(entries_)allocator_.release(entries_,allocator_.context);entries_=replacement;capacity_=next;return true;
 }
public:
 struct Insertion {bool second;};
 explicit CaptureSet(PageAllocator allocator={})noexcept:allocator_(allocator.allocate&&allocator.release?allocator:PageAllocator{}){}
 ~CaptureSet(){if(entries_)allocator_.release(entries_,allocator_.context);}
 CaptureSet(const CaptureSet&)=delete;CaptureSet&operator=(const CaptureSet&)=delete;
 bool valid()const{return !failed_;}size_t capacity()const{return capacity_;}
 void clear(){size_=0;failed_=false;if(!++generation_){for(size_t i=0;i<capacity_;++i)entries_[i].generation=0;generation_=1;}}
 bool count(uint32_t address)const{return capacity_&&entries_[slot(address)].generation==generation_;}
 Insertion insert(uint32_t address){
  if(failed_)return {false};if(capacity_&&entries_[slot(address)].generation==generation_)return {false};
  if((!capacity_||size_>=capacity_/2)&&!grow())return {false};entries_[slot(address)]={address,generation_};++size_;return {true};
 }
};
// Recently missed exact consumer field, not stale data. The acquisition
// boundary can fill this range before broad page subscriptions. A caller may
// store its last observed tick to bound the hint's lifetime.
struct CaptureScratch {
 CaptureSet actors,types,small,houses;
 std::vector<uint32_t>queue,list,docks,warmPages,warmSortScratch,technoRoster;
 Key technoKey{};uint32_t technoItems=0;int technoCount=-1;
 // A saved roster is only a pointer LIST, never a native command grant.
 // Match every source identity dimension before reusing its bytes.
 const uint32_t* capturedTechnoRoster(const Key&key,uint32_t items,int count)const{
  if(count<0||count>16384||count!=technoCount||items!=technoItems||
     !technoKey.sameFrame(key)||technoKey.serial!=key.serial||
     technoKey.tick!=key.tick||technoRoster.size()!=size_t(count))return nullptr;
  return technoRoster.data();
 }
 // Retained worker-owned buffer: enough to read 32 contiguous actor pages
 // with one OS call. No allocation, sort, or page scan on game callbacks.
 std::array<uint8_t,32*PageSize>pageScratch{};
 explicit CaptureScratch(PageAllocator allocator={}):actors(allocator),types(allocator),small(allocator),houses(allocator){}
 void reset(){actors.clear();types.clear();small.clear();houses.clear();queue.clear();list.clear();docks.clear();warmPages.clear();warmSortScratch.clear();technoRoster.clear();technoKey={};technoItems=0;technoCount=-1;}
 bool valid()const{return actors.valid()&&types.valid()&&small.valid()&&houses.valid();}
};
// These layouts are inherited from the supported YR readers, not new native
// entry points. Capture discovers pointer dependencies without invoking any
// engine routine or modifying the game. Per-feature projections run after seal.
template<class Read>class CaptureGraph {
 Snapshot&frame_;Read live_;
 struct ScratchDeleter{void operator()(CaptureScratch*p)const{if(p){p->~CaptureScratch();std::free(p);}}};
 std::unique_ptr<CaptureScratch,ScratchDeleter>owned_;CaptureScratch*scratch_=nullptr;
 uint32_t demand_=Base;bool failed_=false;
 bool working(){if(!failed_&&(!scratch_||!scratch_->valid())){failed_=true;++frame_.counters.overflows;}return !failed_&&!frame_.overflow();}
 template<class T>T value(uint32_t at,T fallback={}){T v{};return frame_.capture(live_,at,&v,sizeof v)?v:fallback;}
 void block(uint32_t p,size_t n){if(working()&&range(p,n))frame_.prefetch(live_,p,n);}
 void enqueue(uint32_t p){if(!working()||p<0x10000||p>=0x80000000u||scratch_->actors.count(p))return;if(scratch_->queue.size()>=65536){failed_=true;++frame_.counters.overflows;return;}scratch_->actors.insert(p);scratch_->queue.push_back(p);}
 void auxiliary(uint32_t p,size_t n){if(working()&&p>=0x10000&&scratch_->small.insert(p).second)block(p,n);}
 void animation(uint32_t p){
  if(!working()||p<0x10000||!scratch_->small.insert(p).second)return;
  block(p,0x400);auxiliary(value<uint32_t>(p+0xC8),0x400);enqueue(value<uint32_t>(p+0xCC));
 }
 void house(uint32_t p){
  if(!working()||p<0x10000||!scratch_->houses.insert(p).second)return;block(p,0x5800);
  auxiliary(value<uint32_t>(p+0x34),0x400);
  for(uint32_t offset:{0x53ACu,0x53B0u,0x53B4u,0x53B8u,0x53BCu,0x53CCu})auxiliary(value<uint32_t>(p+offset),0x80);
 }
 void type(uint32_t p){
  if(!working()||p<0x10000||!scratch_->types.insert(p).second)return;block(p,0x2000);auxiliary(value<uint32_t>(p+0x60),128);
  if(demand_&Ore){auto&docks=scratch_->docks;if(vector(p+0x3E8,256,docks))for(uint32_t dock:docks)block(dock,0x2000);}
  if(!(demand_&Combat))return;
  for(unsigned slot=0;slot<18;++slot)for(uint32_t base:{0x898u,0xA94u}){
   if(!working())return;
   const uint32_t weapon=value<uint32_t>(p+base+slot*28);if(weapon<0x10000||!scratch_->small.insert(weapon).second)continue;
   block(weapon,0x200);auxiliary(value<uint32_t>(weapon+0xA0),0x400);auxiliary(value<uint32_t>(weapon+0xAC),0x200);
  }
 }
 void actor(uint32_t p){
  block(p,0x800);const uint32_t vt=value<uint32_t>(p);
  const int kind=vt==0x7F5C70?2:vt==0x7EB058?1:vt==0x7E3EBC?3:vt==0x7E22A4?0:-1;
  if(kind<0)return;type(value<uint32_t>(p+(kind==3?0x520:kind==1?0x6C0:0x6C4)));house(value<uint32_t>(p+0x21C));
  // A visual publication must include the native disguise as well as the
  // real type. Main-view replacement needs BOTH bounds from the same epoch.
  if((demand_&Visual)&&(kind==1||kind==2)&&value<uint8_t>(p+0x1D8)){
   const uint32_t disguise=value<uint32_t>(p+0x518),table=value<uint32_t>(disguise);
   if(table==0x7F6218||table==0x7EB610||table==0x7E2868||table==0x7E4570)type(disguise);
   else auxiliary(disguise,0x400); // TerrainType; do not read a TechnoType-sized object.
   house(value<uint32_t>(p+0x51C));
  }
  if(kind==3&&(demand_&Visual)){
   for(unsigned i=0;i<21;++i)animation(value<uint32_t>(p+0x55C+i*4));
   for(unsigned i=0;i<8;++i)animation(value<uint32_t>(p+0x5C8+i*4));
  }
  // Original passenger links and object references, including targets not in
  // the main Techno inventory. Cycles/address duplicates enter the queue once.
  if(demand_&(Passengers|Visual))for(uint32_t at:{0x30u,0x118u,0x11Cu})enqueue(value<uint32_t>(p+at));
  if(demand_&(Combat|Ore))for(uint32_t at:{0x218u,0x2ACu,0x2B4u,0x5A4u})enqueue(value<uint32_t>(p+at));
  const uint32_t radios=value<uint32_t>(p+0xE4);const int radioCount=value<int>(p+0xE8,-1);
  if((demand_&(Passengers|Ore|Combat))&&radios>=0x10000&&radioCount>0&&radioCount<=256){block(radios,size_t(radioCount)*4);for(int i=0;i<radioCount;++i)enqueue(value<uint32_t>(radios+i*4));}
  if(demand_&(Combat|Visual))for(uint32_t offset:{0x274u,0x278u}){
   uint32_t temporal=value<uint32_t>(p+offset);std::array<uint32_t,52>seen;unsigned count=0;
   while(temporal>=0x10000&&count<seen.size()){
    if(std::find(seen.begin(),seen.begin()+count,temporal)!=seen.begin()+count)break;
    seen[count++]=temporal;block(temporal,0x60);temporal=value<uint32_t>(temporal+0x44);
   }
  }
 }
public:
 CaptureGraph(Snapshot&frame,Read live,CaptureScratch&scratch):frame_(frame),live_(live),scratch_(&scratch){scratch_->reset();}
 // Standalone callers may own short-lived scratch. Runtime passes its retained
 // owner explicitly, so this allocation never runs on the production tick.
 CaptureGraph(Snapshot&frame,Read live):frame_(frame),live_(live){
  if(void*p=std::malloc(sizeof(CaptureScratch))){scratch_=::new(p)CaptureScratch;owned_.reset(scratch_);}
 }
 bool vector(uint32_t address,int limit,std::vector<uint32_t>&out,uint32_t*itemsOut=nullptr){
  struct Vector {uint32_t vt,items;int capacity;uint32_t flags;int count,step;}v{};
  out.clear();++frame_.counters.rosterCaptures;
  if(!frame_.capture(live_,address,&v,sizeof v)||v.count<0||v.count>limit||v.capacity<v.count||v.capacity>262144||(v.count&&v.items<0x10000))return false;
  out.resize(size_t(v.count));
  if(v.count&&!frame_.capture(live_,v.items,out.data(),out.size()*4))return false;
  if(itemsOut)*itemsOut=v.items; // Publish only after complete, immutable roster.
  return true;
 }
 bool capture(uint32_t demand,const std::vector<uint32_t>&requests,
              const RequestedSpan*exact=nullptr,size_t exactCount=0){
  if(!working())return false;
  demand_=demand|Base;const bool visuals=(demand_&Visual)!=0;
  // Only a confirmed miss records exact bytes. A full-page read may fail on a
  // transient allocation boundary even when a specific field is readable.
  // Try bounded exact ranges BEFORE normal graph discovery and broad
  // page demands, so neither can mark their bytes failed first.
  // This remains the same single unsealed acquisition: no live fallback in
  // an already sealed consumer or a second capture of the game frame.
  size_t exactBytes=0,exactReads=0;
  constexpr size_t ExactBudget=RecoveryConfirmBytes,ExactMaxReads=RecoveryConfirmSpans;
  // Rotate the ring start across frames so persistent misses do not starve
  // other active fields under a strict maximum number of live calls.
  for(size_t step=0;exact&&step<exactCount&&exactReads<ExactMaxReads;++step){
   const auto&span=exact[(step+(frame_.key.serial%exactCount))%exactCount];
   if(!span.bytes||span.bytes>256||!range(span.address,span.bytes)||
      uint32_t(frame_.key.tick-span.tick)>1000||exactBytes+span.bytes>ExactBudget)continue;
   exactBytes+=span.bytes;++exactReads;
   frame_.prefetch(live_,span.address,span.bytes);
   if(!working())return false;
  }
  // Only a non-command Host display consumer, AND an explicit producer
  // admission check, may request this boundary-only read set. The external
  // VisualCache performs its own full Graph discovery on the Host thread.
  // Do not capture unit/Type/Weapon/House object graphs here: no command
  // admission can use a boundary-only sample as its full actor census.
  if(demand_==uint32_t(Base|HostDisplayOnly)){
   for(uint32_t p:{0xA80208u,0xA83CE0u,0xA8B230u,0xA8BC60u,0xA8E348u,
                   0xA8ED84u,0x8871E0u,0x887324u,0x8A00A4u,0x8B4108u,
                   0xAC1198u,0xB0CD48u})block(p,128);
   block(0x87F7E8,0x1500);
   block(frame_.key.scenario,0x1300);
   // A game-thread read-only scene has no native commands. The Host already
   // owns lazy display page subscriptions: don't pull these page ranges
   // back onto the game's producer solely to paint an overlay.
   return working();
  }
  // Global fields are one read per overlapping page, regardless of which
  // collector originally owned them. No Selected object list is frozen here.
  for(uint32_t p:{0xA80208u,0xA83CE0u,0xA8B230u,0xA8BC60u,0xA8E348u,0xA8ED84u,0x8871E0u,0x887324u,0x8A00A4u,0x8B4108u,0xAC1198u,0xB0CD48u})block(p,128);
  block(0x87F7E8,0x1500);block(frame_.key.scenario,0x1300);house(frame_.key.house);
  // Resource consumers share the native sidebar's four cameo strips with
  // Runtime instead of faulting these pages into an independent Host scan.
  if(demand_&Resources)block(0x87F7E8+0x1500,0x53A0-0x1500);
  auxiliary(value<uint32_t>(0x887324),0xD0);auxiliary(value<uint32_t>(0x8871E0),0x1000);
  if(!working())return false;auto&list=scratch_->list;
  for(uint32_t table:{0xA8EC78u,0xA8EB40u,0x8B4108u}){
   if(table==0x8B4108&&!(demand_&Ore))continue;
   uint32_t capturedItems=0;
   if(!vector(table,16384,list,table==0xA8EC78u?&capturedItems:nullptr))return false;
   if(table==0xA8EC78u){
    // Retain the exact first-stage inventory for the later game-thread
    // identity index. std::vector::swap moves ownership in O(1): no second
    // 16K-pointer copy, no extra game read, no stale bytes after reset().
    scratch_->technoRoster.swap(list);
    scratch_->technoKey=frame_.key;
    scratch_->technoItems=capturedItems;
    scratch_->technoCount=int(scratch_->technoRoster.size());
    for(uint32_t p:scratch_->technoRoster)enqueue(p);
   }else for(uint32_t p:list)enqueue(p);
  }
  if(!vector(0xA80228,32,list))return false;for(uint32_t p:list)house(p);
  if(demand_&(Bullets|Visual)){
   if(!vector(0xA8ED40,16384,list))return false;
   for(uint32_t p:list){if(!working())return false;block(p,0x200);auxiliary(value<uint32_t>(p+0xAC),0x400);enqueue(value<uint32_t>(p+0xB0));enqueue(value<uint32_t>(p+0x10C));}
  }
  // The map pointer inventory is shared; mutable Cell pages are selected by
  // actual consumer demand, not an unconditional per-frame whole-map sweep.
  if((demand_&(Visual|Ore))&&frame_.key.cellCount&&frame_.key.cellCount<=512u*512u)block(frame_.key.cells,size_t(frame_.key.cellCount)*4);
  if(demand_&Ore){
   for(uint32_t table:{0xA83CE0u,0xA83D80u,0xB0F4E8u}){
    if(!vector(table,4096,list))continue;
    for(uint32_t p:list){if(!working())return false;if(table==0xA83CE0)type(p);else block(p,0x400);
     if(table==0xB0F4E8)auxiliary(value<uint32_t>(p+0xE0),0x400);}
   }block(0x89EA40,12*9*sizeof(float));
  }
  if(visuals){
   for(uint32_t table:{0xA8E988u,0xA8E9A8u,0xA83DC8u,0xA80208u,0x8B4190u,0xABC878u,0xB04A60u,0xA8EC38u,0xAC1678u,0x88A0F0u,0xABCB78u,0x887388u,0x8B3D10u,0xB04DA8u,0x8A0E88u}){
    if(!vector(table,65536,list))continue;
    for(uint32_t p:list){if(!working())return false;block(p,0x400);
     if(table==0xA8E988)auxiliary(value<uint32_t>(p+0xC8),0x400);
     if(table==0xA8E9A8)animation(p);
     if(table==0x887388){auxiliary(value<uint32_t>(p+0x104),0x400);house(value<uint32_t>(p+0x10C));}
     if(table==0x8B3D10){const int count=value<int>(p+0x6C),capacity=value<int>(p+0x64);const uint32_t items=value<uint32_t>(p+0x60);
      if(count>0&&count<=64&&capacity>=count&&capacity<=4096){block(items,size_t(count)*16);for(int i=0;i<count;++i)auxiliary(value<uint32_t>(items+i*16),0x400);}}
     if(table==0xA83DC8||table==0xA80208)auxiliary(value<uint32_t>(p+0xAC),0x400);
     if(table==0xA8EC38){const int count=value<int>(p+0x148);if(count>=3&&count<=6)block(value<uint32_t>(p+0x14C),size_t(count)*8);}
    }
   }
   for(uint32_t layer=0;layer<5;++layer)if(vector(0x8A0360+layer*24,65536,list))for(uint32_t p:list)enqueue(p);
   block(0x89C3B0,96);for(unsigned i=0;i<24;++i)auxiliary(value<uint32_t>(0x89C3B0+i*4),0x114);
  }
  // The published roster already identifies the next actor ranges. Warm
  // adjacent, unobserved pages in bounded reads instead of one OS call per
  // actor page. A failed speculative span leaves the original partial reads
  // untouched; pointer-followed actors still use the normal discovery path.
  auto&warm=scratch_->warmPages;
  for(uint32_t p:scratch_->queue)if(range(p,0x800))
   for(uint32_t page=p&~4095u;uint64_t(page)<uint64_t(p)+0x800;page+=PageSize)warm.push_back(page);
  sortUniquePageAddresses(warm,scratch_->warmSortScratch);
  frame_.prefetchPages(live_,warm,scratch_->pageScratch.data(),scratch_->pageScratch.size(),false,32);
  for(size_t i=0;i<scratch_->queue.size();++i){if(!working())return false;actor(scratch_->queue[i]);}
  if(!working())return false;
  // Required Host pages keep tighter 16-page fault isolation.
  frame_.prefetchPages(live_,requests,scratch_->pageScratch.data(),scratch_->pageScratch.size(),true,16);
  return working();
 }
};
}
