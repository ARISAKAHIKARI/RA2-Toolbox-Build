#pragma once
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <deque>
#include <vector>
#include <algorithm>
#include "process_connection.h"
namespace external {
// Read-only handle. No allocation, injection, protection or write API for the game.
struct ReaderDiagnostics {
 uint32_t reads=0,errors=0,pageHits=0,pageMisses=0,pageFallbacks=0,prefetchedPages=0,prefetchMisses=0;
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
 struct Page {std::array<unsigned char,4096> bytes;uint64_t epoch=0;};
 HANDLE query_{};std::unordered_map<uint32_t,Page> pages_;std::deque<uint32_t> pageOrder_;
 uint64_t epoch_=0;size_t pageAllocations_=0;
 // A partly unreadable page cannot fill the cache. Retry its fields directly
 // for this sample instead of repeating the failing 4 KiB read for each one.
 std::unordered_set<uint32_t> failedPages_;
 std::vector<uint32_t> prefetchPages_;
 auto storage(uint32_t page)->decltype(pages_)::iterator{
  auto it=pages_.find(page);if(it!=pages_.end())return it;
  if(pages_.size()==8192){auto node=pages_.extract(pageOrder_.front());pageOrder_.pop_front();node.key()=page;node.mapped().epoch=0;it=pages_.insert(std::move(node)).position;}
  else{it=pages_.try_emplace(page).first;++pageAllocations_;}
  pageOrder_.push_back(page);return it;
 }
 const Page* cachedPage(uint32_t page){
  auto it=pages_.find(page);if(it!=pages_.end()&&it->second.epoch==epoch_){++pageHits;return &it->second;}
  ++pageMisses;if(failedPages_.count(page))return nullptr;
  if(it==pages_.end())it=storage(page);
  auto&cached=it->second;cached.epoch=0;SIZE_T got=0;++reads;
  if(ReadProcessMemory(connection_->handle,(void*)(uintptr_t)page,cached.bytes.data(),cached.bytes.size(),&got)&&got==cached.bytes.size()){
   cached.epoch=epoch_;return &cached;
  }
  if(failedPages_.size()<8192)failedPages_.insert(page);return nullptr;
 }
public:
 ProcessReader()=default;ProcessReader(const ProcessReader&)=delete;
 ProcessReader& operator=(const ProcessReader&)=delete;
 ~ProcessReader(){close();}
 bool open(DWORD pid,bool allowQueryOnly=false){
  if(connection_&&connection_->pid==pid&&connection_->readable&&connection_->alive()){begin();return true;}
  close();lastOpenError=lastRegionQueryError=0;
  connection_=acquireConnection(pid,true,lastOpenError);
  if(!connection_&&allowQueryOnly)connection_=acquireConnection(pid,false,lastOpenError);
  return valid();
 }
 void close(){if(query_)CloseHandle(query_);query_=nullptr;connection_.reset();pages_.clear();pageOrder_.clear();begin();}
 bool valid()const{return bool(connection_);}
 bool alive()const{return connection_&&connection_->alive();}
 bool canRead()const{return connection_&&connection_->readable;}
 DWORD pid()const{return connection_?connection_->pid:0;}
 uint64_t generation()const{return connection_?connection_->generation:0;}
 std::shared_ptr<const ProcessConnection> connectionIdentity()const{return connection_;}
 bool imagePath(wchar_t*out,DWORD*size)const{return connection_&&QueryFullProcessImageNameW(connection_->handle,0,out,size);}
 // Keep address-to-storage bindings between observations, never their old
 // values. The first access in a new epoch refills the page from the game.
 // This avoids freeing/allocating thousands of 4 KiB nodes every frame.
 void begin(){if(!++epoch_){pages_.clear();pageOrder_.clear();epoch_=1;}failedPages_.clear();reads=errors=pageHits=pageMisses=pageFallbacks=prefetchedPages=prefetchMisses=0;lastReadError=0;lastFailedAddress=0;lastFailedSize=0;lastBytesRead=0;lastRegionState=lastRegionProtect=0;lastRegionQueried=false;lastRegionQueryError=0;}
 size_t cachedPageStorage()const{return pages_.size();}
 size_t pageBufferAllocations()const{return pageAllocations_;}
 // Adjacent actor pages enter this epoch in one bounded RPM call. A failed
 // speculative span poisons no page: normal per-page/field fallback remains.
 void prefetch(const std::vector<uint32_t>&addresses,size_t span){
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
  for(size_t first=0;first<wanted.size();){size_t end=first+1;while(end<wanted.size()&&end-first<16&&wanted[end]==wanted[end-1]+4096)++end;
   // Isolated pages are already optimal in the ordinary lazy reader.
   if(end-first>1){SIZE_T got=0;++reads;size_t n=(end-first)*4096;
    if(ReadProcessMemory(connection_->handle,(void*)(uintptr_t)wanted[first],bytes.data(),n,&got)&&got==n){
     for(size_t i=first;i<end;++i){auto&cached=storage(wanted[i])->second;std::memcpy(cached.bytes.data(),bytes.data()+(i-first)*4096,4096);cached.epoch=epoch_;++prefetchedPages;}
    }else ++prefetchMisses;
   }first=end;
  }
 }
 bool read(uint32_t address,void*out,size_t size){
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
struct ReadView {ProcessReader*reader;bool operator()(uint32_t p,void*out,size_t n)const{return reader->read(p,out,n);}};
struct NoResolve {uint32_t operator()(uint32_t)const{return 0;}};
}
