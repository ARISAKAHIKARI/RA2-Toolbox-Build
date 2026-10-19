#pragma once
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <array>
#include "process_connection.h"
namespace external {
// Read-only handle. No allocation, injection, protection or write API for the game.
struct ReaderDiagnostics {
 uint32_t reads=0,errors=0,pageHits=0,pageMisses=0,pageFallbacks=0;
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
 HANDLE query_{};std::unordered_map<uint32_t,std::array<unsigned char,4096>> pages_;
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
 void close(){if(query_)CloseHandle(query_);query_=nullptr;connection_.reset();begin();}
 bool valid()const{return bool(connection_);}
 bool alive()const{return connection_&&connection_->alive();}
 bool canRead()const{return connection_&&connection_->readable;}
 DWORD pid()const{return connection_?connection_->pid:0;}
 uint64_t generation()const{return connection_?connection_->generation:0;}
 std::shared_ptr<const ProcessConnection> connectionIdentity()const{return connection_;}
 bool imagePath(wchar_t*out,DWORD*size)const{return connection_&&QueryFullProcessImageNameW(connection_->handle,0,out,size);}
 void begin(){pages_.clear();reads=errors=pageHits=pageMisses=pageFallbacks=0;lastReadError=0;lastFailedAddress=0;lastFailedSize=0;lastBytesRead=0;lastRegionState=lastRegionProtect=0;lastRegionQueried=false;lastRegionQueryError=0;}
 bool read(uint32_t address,void*out,size_t size){
  if(!canRead()||!out||address<0x10000||!size||size>4*1024*1024||uint64_t(address)+size>0x80000000ull){
   ++errors;lastReadError=!valid()?ERROR_INVALID_HANDLE:!canRead()?ERROR_ACCESS_DENIED:ERROR_INVALID_PARAMETER;
   lastFailedAddress=address;lastFailedSize=size;lastBytesRead=0;lastRegionQueried=false;return false;
  }
  uint32_t page=address&~4095u;
  if(size<=4096&&(address&4095)+size<=4096){
   auto it=pages_.find(page);if(it!=pages_.end()){++pageHits;std::memcpy(out,it->second.data()+(address&4095),size);return true;}
   ++pageMisses;
   if(pages_.size()<8192){std::array<unsigned char,4096> bytes;SIZE_T got=0;++reads;
    if(ReadProcessMemory(connection_->handle,(void*)(uintptr_t)page,bytes.data(),bytes.size(),&got)&&got==bytes.size()){
     auto result=pages_.emplace(page,std::move(bytes));std::memcpy(out,result.first->second.data()+(address&4095),size);return true;
    }
   }
  }
  if(size<=4096&&(address&4095)+size<=4096)++pageFallbacks;
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
