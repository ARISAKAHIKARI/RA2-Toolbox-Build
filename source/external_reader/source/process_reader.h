#pragma once
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <list>
#include <vector>
#include <algorithm>
#include <new>
#include "process_connection.h"
#include "read_page_budget.h"
#include "../../shared/source/world_frame_transport.h"
namespace external {
// Read-only handle. No allocation, injection, protection or write API for the game.
struct ReaderDiagnostics {
 uint32_t reads=0,errors=0,pageHits=0,pageMisses=0,pageFallbacks=0,prefetchedPages=0,prefetchMisses=0,pageEvictions=0,pageCurrentEvictions=0,pageBudgetDenials=0,pageAllocationFailures=0,emergencyPageReads=0,pagePressureRetired=0;
 // Read-only diagnostics: do not change access rights or game memory.
 DWORD lastOpenError=0,lastReadError=0,lastRegionQueryError=0;
 uint32_t lastFailedAddress=0;
 size_t lastFailedSize=0;
 SIZE_T lastBytesRead=0;
 DWORD lastRegionState=0,lastRegionProtect=0;
 bool lastRegionQueried=false;
};
// A session belongs to one sampling task/thread. Parallel tasks use separate
// sessions; only the immutable connection is shared, never cache/diagnostics.
class ProcessReader : public ReaderDiagnostics {
 std::shared_ptr<const ProcessConnection> connection_;
 std::shared_ptr<worldframe::Client> worldClient_;worldframe::Token worldFrame_;
 uint32_t worldDemand_=worldframe::Base;
 uint32_t worldSubscribedAt_=0;
 std::array<uint64_t,8192> worldDemandBits_{};std::vector<uint32_t> worldDemands_;bool worldComplete_=true;
 bool frameRead(uint32_t address,void*out,size_t size,bool optional=false){
  if(!out||!worldframe::range(address,size)){++errors;if(!optional)worldComplete_=false;return false;}
  for(uint32_t page=address&~4095u;uint64_t(page)<uint64_t(address)+size;page+=4096){
   const uint32_t index=page>>12;const uint64_t mask=uint64_t(1)<<(index&63);
   if(!(worldDemandBits_[index>>6]&mask)){worldDemandBits_[index>>6]|=mask;worldDemands_.push_back(page);}
  }
  if(worldFrame_->read(address,out,size)){++pageHits;return true;}
  ++errors;if(!optional||!worldFrame_->observed(address,size))worldComplete_=false;return false;
 }
 using PageOrder=std::list<uint32_t>;
 struct Page {std::array<unsigned char,4096> bytes;uint64_t epoch=0;PageOrder::iterator order;};
 HANDLE query_{};std::unordered_map<uint32_t,Page> pages_;PageOrder pageOrder_;
 readpages::Budget*pageBudget_;const size_t pageLimit_;
 size_t observedPressure_=0;
 // Fixed read scratch keeps adjacent fields batched if a new session cannot
 // admit its first heap page. It is not part of the dynamic cache pool.
 Page emergency_;uint32_t emergencyAddress_=0;
 bool growthBlocked_=false,failedTrackingBlocked_=false;
 uint64_t epoch_=0;size_t pageAllocations_=0;
 // A partly unreadable page cannot fill the cache. Retry its fields directly
 // for this sample instead of repeating the failing 4 KiB read for each one.
 std::unordered_set<uint32_t> failedPages_;
 std::vector<uint32_t> prefetchPages_;
 void clearPages(){const auto count=pages_.size();pages_.clear();pageOrder_.clear();pageBudget_->release(count);emergency_.epoch=0;}
 size_t trimUnderPressure(){
  const auto pressure=pageBudget_->pressure();if(pressure==observedPressure_)return 0;observedPressure_=pressure;
  // Only this session's owner touches its containers. Keep every page used
  // in the just-finished observation and a small reusable floor (up to4MiB).
  // The ordered prefix contains only older/failed pages, so no normal hot
  // path scan or per-hit bookkeeping is needed.
  const size_t floor=std::min<size_t>(1024,pageLimit_/4);size_t retired=0;
  while(pages_.size()>floor&&!pageOrder_.empty()){
   auto found=pages_.find(pageOrder_.front());if(found->second.epoch==epoch_)break;
   pages_.erase(found);pageOrder_.pop_front();++retired;
  }
  pageBudget_->release(retired);return retired;
 }
 auto storage(uint32_t page)->decltype(pages_)::iterator{
  auto it=pages_.find(page);if(it!=pages_.end())return it;
  if(pages_.size()<pageLimit_&&!growthBlocked_){
   if(!pageBudget_->acquire()){++pageBudgetDenials;growthBlocked_=true;}
   else{
   // An unread/failed slot belongs before current pages. Only a successful
   // page fill below promotes it, including fills through prefetch().
    bool ordered=false;
    try{
     pageOrder_.push_front(page);ordered=true;it=pages_.try_emplace(page).first;
     it->second.order=pageOrder_.begin();++pageAllocations_;return it;
    }catch(const std::bad_alloc&){if(ordered)pageOrder_.pop_front();pageBudget_->release(1);++pageAllocationFailures;growthBlocked_=true;}
    catch(...){if(ordered)pageOrder_.pop_front();pageBudget_->release(1);throw;}
   }
  }
  // A shared-budget or allocation failure is retried next observation, not
  // per field. Existing map/list nodes continue to recycle without allocation.
  if(pageOrder_.empty())return pages_.end();
  auto slot=pageOrder_.begin();auto node=pages_.extract(*slot);++pageEvictions;if(node.mapped().epoch==epoch_)++pageCurrentEvictions;
  *slot=page;node.key()=page;node.mapped().epoch=0;return pages_.insert(std::move(node)).position;
 }
 void current(Page&cached){cached.epoch=epoch_;pageOrder_.splice(pageOrder_.end(),pageOrder_,cached.order);}
 const Page* cachedPage(uint32_t page){
  auto it=pages_.find(page);if(it!=pages_.end()&&it->second.epoch==epoch_){++pageHits;return &it->second;}
  if(emergency_.epoch==epoch_&&emergencyAddress_==page){++pageHits;return &emergency_;}
  ++pageMisses;if(failedPages_.count(page))return nullptr;
  if(it==pages_.end())it=storage(page);
  const bool pooled=it!=pages_.end();auto&cached=pooled?it->second:emergency_;cached.epoch=0;SIZE_T got=0;++reads;if(!pooled)++emergencyPageReads;
  if(ReadProcessMemory(connection_->handle,(void*)(uintptr_t)page,cached.bytes.data(),cached.bytes.size(),&got)&&got==cached.bytes.size()){
   if(pooled)current(cached);else{cached.epoch=epoch_;emergencyAddress_=page;}return &cached;
  }
  if(!failedTrackingBlocked_&&failedPages_.size()<readpages::SessionPages)try{failedPages_.insert(page);}catch(const std::bad_alloc&){failedTrackingBlocked_=true;}
  return nullptr;
 }
public:
 explicit ProcessReader(readpages::Budget&budget=readpages::sharedBudget(),size_t limit=readpages::SessionPages):pageBudget_(&budget),pageLimit_(std::min(limit,readpages::SessionPages)){}ProcessReader(const ProcessReader&)=delete;
 ProcessReader& operator=(const ProcessReader&)=delete;
 ~ProcessReader(){close();}
 bool open(DWORD pid,bool allowQueryOnly=false){
  if(connection_&&connection_->pid==pid&&connection_->readable&&connection_->alive()){begin();return true;}
  close();lastOpenError=lastRegionQueryError=0;
  connection_=acquireConnection(pid,true,lastOpenError);
  if(!connection_&&allowQueryOnly)connection_=acquireConnection(pid,false,lastOpenError);
  return valid();
 }
 void close(){endWorldFrame();worldClient_.reset();if(query_)CloseHandle(query_);query_=nullptr;connection_.reset();clearPages();begin();}
 bool valid()const{return bool(connection_);}
 bool alive()const{return connection_&&connection_->alive();}
 bool canRead()const{return connection_&&connection_->readable;}
 DWORD pid()const{return connection_?connection_->pid:0;}
 uint64_t generation()const{return connection_?connection_->generation:0;}
 std::shared_ptr<const ProcessConnection> connectionIdentity()const{return connection_;}
 bool beginWorldFrame(uint32_t match,worldframe::Token frame={}){
  endWorldFrame();if(!canRead())return false;
  if(!worldClient_)worldClient_=worldframe::acquireClient(pid(),generation());
  worldClient_->subscribe(uintptr_t(this),worldDemand_);
  if(!frame)frame=worldClient_->acquire(match);else if(!worldClient_->accepts(frame))return false;
  if(!frame||!frame->sealed()||frame->key.pid!=pid()||frame->key.owner!=GetCurrentProcessId()||(match&&frame->key.match!=match))return false;
  worldFrame_=std::move(frame);worldComplete_=true;worldSubscribedAt_=GetTickCount();return true;
 }
 bool unchangedWorldFrame(const worldframe::Token&frame,uint32_t now){
  // Full connection/liveness and demand maintenance still runs at least
  // every 50 ms, including paused games. Never extend a source timestamp.
  return worldClient_&&frame&&frame->key.pid==pid()&&
   ((uint64_t(frame->key.connectionHigh)<<32)|frame->key.connectionLow)==generation()&&
   uint32_t(now-worldSubscribedAt_)<50&&worldClient_->unchanged(frame,now);
 }
 void endWorldFrame(){
  if(worldClient_&&!worldDemands_.empty())worldClient_->request(worldDemands_);
  for(uint32_t page:worldDemands_){const uint32_t index=page>>12;worldDemandBits_[index>>6]=0;}
  worldDemands_.clear();worldFrame_.reset();worldComplete_=true;
 }
 bool worldFrameActive()const{return bool(worldFrame_);}
 void setWorldDemand(uint32_t demand){worldDemand_=demand|worldframe::Base;}
 uint32_t worldDemand()const{return worldDemand_;}
 void markWorldIncomplete(){worldComplete_=false;}
 bool worldFrameComplete()const{return worldFrame_&&worldComplete_;}
 uint32_t worldFrameSerial()const{return worldFrame_?worldFrame_->key.serial:0;}
 int worldLogicFrame()const{return worldFrame_?worldFrame_->key.frame:-1;}
 worldframe::Token worldFrameToken()const{return worldFrame_;}
 bool imagePath(wchar_t*out,DWORD*size)const{return connection_&&QueryFullProcessImageNameW(connection_->handle,0,out,size);}
 // Keep address-to-storage bindings between observations, never their old
 // values. The first access in a new epoch refills the page from the game.
 // This avoids freeing/allocating thousands of 4 KiB nodes every frame.
 void begin(){const auto retired=trimUnderPressure();if(!++epoch_){clearPages();epoch_=1;}emergency_.epoch=0;growthBlocked_=failedTrackingBlocked_=false;failedPages_.clear();reads=errors=pageHits=pageMisses=pageFallbacks=prefetchedPages=prefetchMisses=pageEvictions=pageCurrentEvictions=pageBudgetDenials=pageAllocationFailures=emergencyPageReads=0;pagePressureRetired=uint32_t(retired);lastReadError=0;lastFailedAddress=0;lastFailedSize=0;lastBytesRead=0;lastRegionState=lastRegionProtect=0;lastRegionQueried=false;lastRegionQueryError=0;}
 size_t cachedPageStorage()const{return pages_.size();}
 size_t pageBufferAllocations()const{return pageAllocations_;}
 // Adjacent actor pages enter this epoch in one bounded RPM call. A failed
 // speculative span poisons no page: normal per-page/field fallback remains.
 void prefetch(const std::vector<uint32_t>&addresses,size_t span){
  if(worldFrame_)return; // Producer already owns the physical read set.
  if(!canRead()||!span||span>4096)return;auto&wanted=prefetchPages_;wanted.clear();
  const size_t count=std::min<size_t>(addresses.size(),65536);wanted.reserve(count*2);
  for(size_t index=0;index<count;++index){uint32_t p=addresses[index];if(p<0x10000||uint64_t(p)+span>0x80000000ull)continue;
   for(uint32_t page=p&~4095u;uint64_t(page)<uint64_t(p)+span;page+=4096){auto it=pages_.find(page);if(it==pages_.end()||it->second.epoch!=epoch_)wanted.push_back(page);}
  }
  std::sort(wanted.begin(),wanted.end());wanted.erase(std::unique(wanted.begin(),wanted.end()),wanted.end());
  // Bound remote work after de-duplication. Packed Cells share many pages;
  // counting their duplicate addresses used to stop half a chunk early.
  if(wanted.size()>4096)wanted.resize(4096);
  std::array<unsigned char,65536> bytes;
  for(size_t first=0;first<wanted.size();){
   if(growthBlocked_&&pages_.empty())break;
   size_t end=first+1;while(end<wanted.size()&&end-first<16&&wanted[end]==wanted[end-1]+4096)++end;
   if(growthBlocked_&&end-first>pages_.size())end=first+pages_.size();
   // Isolated pages are already optimal in the ordinary lazy reader.
   if(end-first>1){SIZE_T got=0;++reads;size_t n=(end-first)*4096;
    if(ReadProcessMemory(connection_->handle,(void*)(uintptr_t)wanted[first],bytes.data(),n,&got)&&got==n){
     for(size_t i=first;i<end;++i){auto found=storage(wanted[i]);if(found==pages_.end())continue;auto&cached=found->second;std::memcpy(cached.bytes.data(),bytes.data()+(i-first)*4096,4096);current(cached);++prefetchedPages;}
    }else ++prefetchMisses;
   }first=end;
  }
 }
 bool read(uint32_t address,void*out,size_t size){
  if(worldFrame_)return frameRead(address,out,size);
  if(!canRead()||!out||address<0x10000||!size||size>4*1024*1024||uint64_t(address)+size>0x80000000ull){
   ++errors;lastReadError=!valid()?ERROR_INVALID_HANDLE:!canRead()?ERROR_ACCESS_DENIED:ERROR_INVALID_PARAMETER;
   lastFailedAddress=address;lastFailedSize=size;lastBytesRead=0;lastRegionQueried=false;return false;
  }
  const uint32_t page=address&~4095u;
  if(size<=4096){
   if(const auto*first=cachedPage(page)){
    const size_t part=std::min<size_t>(size,4096-(address&4095));
    std::memcpy(out,first->bytes.data()+(address&4095),part);
    if(part==size)return true;
    // A <=4 KiB object block may straddle two prefetched pages. Copy the
    // first before looking up the next: bounded storage may evict it.
    if(const auto*second=cachedPage(page+4096)){
     std::memcpy(static_cast<unsigned char*>(out)+part,second->bytes.data(),size-part);return true;
    }
   }
   ++pageFallbacks;
  }
  return readFresh(address,out,size);
 }
 // Immediate dynamic fields bypass page caching. Preserve the cursor's existing
 // 32 MiB resource limit, without increasing the normal cached read limit.
 bool readFresh(uint32_t address,void*out,size_t size){
  if(worldFrame_)return frameRead(address,out,size);
  return readLive(address,out,size);
 }
 bool readOptional(uint32_t address,void*out,size_t size){return worldFrame_?frameRead(address,out,size,true):read(address,out,size);}
 // Explicit non-sample domain: current permission/UI guards and immutable
 // resource decoders. World consumers never use this as a missing-page fallback.
 bool readLive(uint32_t address,void*out,size_t size){
  if(!canRead()||!out||address<0x10000||!size||size>32*1024*1024||uint64_t(address)+size>0x80000000ull){
   ++errors;lastReadError=!valid()?ERROR_INVALID_HANDLE:!canRead()?ERROR_ACCESS_DENIED:ERROR_INVALID_PARAMETER;
   lastFailedAddress=address;lastFailedSize=size;lastBytesRead=0;lastRegionQueried=false;return false;
  }
  SIZE_T got=0;++reads;BOOL success=ReadProcessMemory(connection_->handle,(void*)(uintptr_t)address,out,size,&got);
  DWORD error=success?ERROR_SUCCESS:GetLastError();
  bool ok=success&&got==size;
  if(!ok){
   ++errors;lastReadError=error?error:ERROR_PARTIAL_COPY;
   lastFailedAddress=address;lastFailedSize=size;lastBytesRead=got;
   MEMORY_BASIC_INFORMATION mbi{};
   // The region-query privilege is optional, acquired only after a failed
   // read. A denied diagnostic handle must never make VM_READ unusable.
   if(!query_&&alive()){query_=OpenProcess(PROCESS_QUERY_INFORMATION,FALSE,pid());
    if(!query_)lastRegionQueryError=GetLastError();}
   lastRegionQueried=query_&&VirtualQueryEx(query_,(void*)(uintptr_t)address,&mbi,sizeof(mbi))==sizeof(mbi);
   if(!lastRegionQueried)lastRegionQueryError=query_?GetLastError():(lastRegionQueryError?lastRegionQueryError:ERROR_ACCESS_DENIED);
   lastRegionState=lastRegionQueried?mbi.State:0;
   lastRegionProtect=lastRegionQueried?mbi.Protect:0;
  }
  return ok;
 }
 template<class T>T get(uint32_t address,T fallback={}){T v{};return read(address,&v,sizeof v)?v:fallback;}
};
class WorldFrameScope {
 ProcessReader&reader_;worldframe::Token previous_;bool previousComplete_=true,bound_=false;
public:
 WorldFrameScope(ProcessReader&reader,uint32_t match,worldframe::Token token={}):reader_(reader),previous_(reader.worldFrameToken()),previousComplete_(!previous_||reader.worldFrameComplete()),bound_(reader.beginWorldFrame(match,std::move(token))){}
 ~WorldFrameScope(){
  const bool sameSource=previous_&&reader_.worldFrameToken()==previous_;
  const bool restoreComplete=previousComplete_&&(!sameSource||reader_.worldFrameComplete());
  reader_.endWorldFrame();if(previous_){const uint32_t match=previous_->key.match;reader_.beginWorldFrame(match,std::move(previous_));if(!restoreComplete)reader_.markWorldIncomplete();}
 }
 explicit operator bool()const{return bound_;}
 bool complete()const{return bound_&&reader_.worldFrameComplete();}
 WorldFrameScope(const WorldFrameScope&)=delete;WorldFrameScope&operator=(const WorldFrameScope&)=delete;
};
struct ReadView {ProcessReader*reader;bool operator()(uint32_t p,void*out,size_t n)const{return reader->read(p,out,n);}};
struct NoResolve {uint32_t operator()(uint32_t)const{return 0;}};
}

