#pragma once
#include "world_frame.h"
#include <windows.h>
#include <mutex>
#include <map>
#include <unordered_map>

namespace worldframe {
constexpr uint32_t Magic=0x57465231u,TransportVersion=2,DataMagic=0x57464432u;
struct DataRef {uint32_t capacity=0,allocation=0;};
struct Slot {
 volatile LONG seq=0;uint32_t count=0;Key key{};Counters counters{};
 DataRef data{};
};
struct Channel {
 uint32_t magic=0,version=0,bytes=0,pid=0,owner=0;
 volatile LONG active=-1;volatile uint32_t heartbeat=0;
 volatile LONG connectionSeq=0;uint32_t connectionLow=0,connectionHigh=0;volatile uint32_t demandMask=0;
 volatile LONG observationSeq=0;Key observed{};uint32_t observedTick=0;
 volatile LONG demandSeq=0;uint32_t demandCount=0,demandRevision=0;
 volatile uint32_t publishFailures=0,publishError=0,publishSystemError=0;
 uint32_t demands[MaxPages];Slot slots[2];
};
// The 32-bit game maps only this small control object until a real publication
// needs payload storage. Each payload buffer grows geometrically and is reused
// only while inactive; readers own mapping handles until their copy completes.
struct DataHeader {
 uint32_t magic=0,version=0,bytes=0,capacity=0;
 uint32_t pid=0,owner=0,slot=0,allocation=0;
 uint32_t generationLow=0,generationHigh=0,connectionLow=0,connectionHigh=0;
 uint32_t reserved[4]{};
};
static_assert(sizeof(Channel)<128u*1024u,"small world control mapping");
static_assert(sizeof(DataHeader)==64&&sizeof(DataHeader)%alignof(Page)==0,"world data wire alignment");
static_assert(!(MaxPages&(MaxPages-1))&&sizeof(DataHeader)+size_t(MaxPages)*sizeof(Page)<0x7fffffffu,"bounded geometric data mapping");
inline bool dataCapacity(uint32_t n){return n&&n<=MaxPages&&!(n&(n-1));}
inline uint32_t grownCapacity(uint32_t needed){if(!needed||needed>MaxPages)return 0;uint32_t n=1;while(n<needed)n*=2;return n;}
inline uint32_t dataBytes(uint32_t capacity){return dataCapacity(capacity)?uint32_t(sizeof(DataHeader)+size_t(capacity)*sizeof(Page)):0;}
inline bool sameData(const DataRef&a,const DataRef&b){return a.capacity==b.capacity&&a.allocation==b.allocation;}
inline void appendNumber(wchar_t*out,size_t&i,uint32_t n){wchar_t digits[10];unsigned at=0;do{digits[at++]=wchar_t(L'0'+n%10);n/=10;}while(n);while(at)out[i++]=digits[--at];}
inline void name(wchar_t*out,uint32_t pid,uint32_t owner){
 const wchar_t*prefix=L"Local\\RA2WorldFrame_v002_";size_t i=0;while(*prefix)out[i++]=*prefix++;
 appendNumber(out,i,pid);out[i++]=L'_';appendNumber(out,i,owner);out[i]=0;
}
inline void dataName(wchar_t*out,const Key&key,uint32_t slot,uint32_t allocation){
 const wchar_t*prefix=L"Local\\RA2WorldData_v002";size_t i=0;while(*prefix)out[i++]=*prefix++;
 for(uint32_t n:{key.pid,key.owner,key.generationLow,key.generationHigh,key.connectionLow,key.connectionHigh,slot,allocation}){out[i++]=L'_';appendNumber(out,i,n);}out[i]=0;
}
inline bool valid(const Channel*c,uint32_t pid,uint32_t owner){return c&&c->magic==Magic&&c->version==TransportVersion&&c->bytes==sizeof(Channel)&&c->pid==pid&&c->owner==owner;}
inline bool connectionMatches(const Channel*c,uint32_t low,uint32_t high){
 if(!c)return false;const LONG seq=c->connectionSeq;if(seq&1)return false;MemoryBarrier();
 const bool same=c->connectionLow==low&&c->connectionHigh==high;MemoryBarrier();return same&&seq==c->connectionSeq;
}
inline bool validData(const DataHeader*h,const Key&key,uint32_t slot,const DataRef&ref){
 return h&&ref.allocation&&dataCapacity(ref.capacity)&&h->magic==DataMagic&&h->version==TransportVersion&&
  h->bytes==dataBytes(ref.capacity)&&h->capacity==ref.capacity&&h->pid==key.pid&&h->owner==key.owner&&h->slot==slot&&h->allocation==ref.allocation&&
  h->generationLow==key.generationLow&&h->generationHigh==key.generationHigh&&h->connectionLow==key.connectionLow&&h->connectionHigh==key.connectionHigh;
}
enum class PublishError:uint32_t {None,Capacity,CreateMapping,MapView,NameCollision,Identity};
class DataView {
 HANDLE mapping_=nullptr;DataHeader*view_=nullptr;
public:
 DataView()=default;~DataView(){close();}DataView(const DataView&)=delete;DataView&operator=(const DataView&)=delete;
 void close(){if(view_)UnmapViewOfFile(view_);view_=nullptr;if(mapping_)CloseHandle(mapping_);mapping_=nullptr;}
 void swap(DataView&other){std::swap(mapping_,other.mapping_);std::swap(view_,other.view_);}
 DataRef ref()const{return view_?DataRef{view_->capacity,view_->allocation}:DataRef{};}
 bool matches(const Key&key,uint32_t slot,const DataRef&ref)const{return validData(view_,key,slot,ref);}
 const Page*pages()const{return view_?reinterpret_cast<const Page*>(reinterpret_cast<const uint8_t*>(view_)+sizeof(DataHeader)):nullptr;}
 Page*pages(){return view_?reinterpret_cast<Page*>(reinterpret_cast<uint8_t*>(view_)+sizeof(DataHeader)):nullptr;}
 bool open(const Key&key,uint32_t slot,const DataRef&ref){
  if(matches(key,slot,ref))return true;
  if(!ref.allocation||!dataCapacity(ref.capacity))return false;
  DataView next;wchar_t path[160];dataName(path,key,slot,ref.allocation);
  next.mapping_=OpenFileMappingW(FILE_MAP_READ,FALSE,path);if(!next.mapping_)return false;
  next.view_=static_cast<DataHeader*>(MapViewOfFile(next.mapping_,FILE_MAP_READ,0,0,dataBytes(ref.capacity)));
  if(!next.matches(key,slot,ref))return false;swap(next);return true;
 }
 PublishError create(const Key&key,uint32_t slot,uint32_t capacity){
  if(!dataCapacity(capacity))return PublishError::Capacity;
  // Shared by producer instances in this module; a name collision is never
  // permission to overwrite another still-pinned allocation.
  static volatile LONG nextAllocation=0;
  for(unsigned attempt=0;attempt<8;++attempt){
   const uint32_t allocation=uint32_t(InterlockedIncrement(&nextAllocation));if(!allocation)continue;
   wchar_t path[160];dataName(path,key,slot,allocation);DataView next;
   next.mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,dataBytes(capacity),path);
   if(!next.mapping_)return PublishError::CreateMapping;
   if(GetLastError()==ERROR_ALREADY_EXISTS)continue;
   next.view_=static_cast<DataHeader*>(MapViewOfFile(next.mapping_,FILE_MAP_ALL_ACCESS,0,0,dataBytes(capacity)));
   if(!next.view_)return PublishError::MapView;
   auto&h=*next.view_;h.version=TransportVersion;h.bytes=dataBytes(capacity);h.capacity=capacity;h.pid=key.pid;h.owner=key.owner;h.slot=slot;h.allocation=allocation;
   h.generationLow=key.generationLow;h.generationHigh=key.generationHigh;h.connectionLow=key.connectionLow;h.connectionHigh=key.connectionHigh;
   MemoryBarrier();h.magic=DataMagic;swap(next);return PublishError::None;
  }return PublishError::NameCollision;
 }
};

// One client and one local immutable copy per process, shared by every Host
// reader. Long render jobs retain their exact source token while Runtime is
// already producing later frames. Neither process waits for a reader lease.
class Client {
 std::mutex mutex_;HANDLE mapping_=nullptr;Channel*channel_=nullptr;
 uint32_t pid_=0,owner_=0;Token latest_;uint32_t copiedSerial_=0;
 std::array<DataView,2>data_;DataRef copiedData_{};LONG copiedSlot_=-1;
 uint64_t generation_=0;
 struct Lease {std::array<uint32_t,6>ticks{};uint32_t used=0;};std::unordered_map<uintptr_t,Lease>leases_;
 std::unordered_map<uint32_t,uint32_t> wanted_;uint32_t publishedAt_=0,prunedAt_=0;
 bool wantedDirty_=false,havePublished_=false,havePruned_=false;
 void pruneRequests(uint32_t now){
  if(havePruned_&&uint32_t(now-prunedAt_)<2)return;
  for(auto it=wanted_.begin();it!=wanted_.end();)if(uint32_t(now-it->second)>250){it=wanted_.erase(it);wantedDirty_=true;}else ++it;
  prunedAt_=now;havePruned_=true;
 }
 void publishRequests(uint32_t now){
  if(!wantedDirty_||(havePublished_&&uint32_t(now-publishedAt_)<2))return;
  InterlockedIncrement(&channel_->demandSeq);uint32_t count=0;
  for(const auto&entry:wanted_)channel_->demands[count++]=entry.first;
  channel_->demandCount=count;++channel_->demandRevision;MemoryBarrier();InterlockedIncrement(&channel_->demandSeq);
  publishedAt_=now;havePublished_=true;wantedDirty_=false;
 }
 void refreshDemand(uint32_t now){
  uint32_t combined=0;for(auto it=leases_.begin();it!=leases_.end();){uint32_t mask=0;for(unsigned bit=0;bit<6;++bit)if((it->second.used&(1u<<bit))&&uint32_t(now-it->second.ticks[bit])<=250)mask|=1u<<bit;
   if(!mask)it=leases_.erase(it);else{combined|=mask;++it;}}
  channel_->demandMask=combined;
 }
 void heartbeat(uint32_t now){
  channel_->heartbeat=now;refreshDemand(now);
  // Polling/another remaining reader does not renew an old visual page set.
  // Idle sweeps are bounded; successful request() calls renew individual pages.
  if(wantedDirty_||!havePruned_||uint32_t(now-prunedAt_)>250)pruneRequests(now);
  publishRequests(now);
 }
 bool open(){
  if(channel_)return valid(channel_,pid_,owner_)&&connectionMatches(channel_,uint32_t(generation_),uint32_t(generation_>>32));wchar_t path[100];name(path,pid_,owner_);
  mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,DWORD(sizeof(Channel)),path);
  if(!mapping_)return false;const bool created=GetLastError()!=ERROR_ALREADY_EXISTS;
  channel_=static_cast<Channel*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(Channel)));
  if(channel_&&created){channel_->active=-1;channel_->version=TransportVersion;channel_->bytes=sizeof(Channel);channel_->pid=pid_;channel_->owner=owner_;MemoryBarrier();channel_->magic=Magic;}
  if(!valid(channel_,pid_,owner_)){close();return false;}
  if(created||channel_->connectionLow!=uint32_t(generation_)||channel_->connectionHigh!=uint32_t(generation_>>32)){
   InterlockedIncrement(&channel_->connectionSeq);InterlockedExchange(&channel_->active,-1);
   channel_->connectionLow=uint32_t(generation_);channel_->connectionHigh=uint32_t(generation_>>32);
   channel_->demandCount=0;channel_->demandRevision=0;channel_->demandMask=0;
   MemoryBarrier();InterlockedIncrement(&channel_->connectionSeq);
  }
  // A fresh Client may reopen a control mapping still owned by Server after
  // the last local reader closed. Its predecessor's demands are not its own.
  InterlockedIncrement(&channel_->demandSeq);channel_->demandCount=0;++channel_->demandRevision;
  MemoryBarrier();InterlockedIncrement(&channel_->demandSeq);channel_->demandMask=0;
  wantedDirty_=!wanted_.empty();havePublished_=havePruned_=false;return true;
 }
 void clearCopy(){latest_.reset();copiedSerial_=0;copiedSlot_=-1;copiedData_={};for(auto&view:data_)view.close();}
 void close(){clearCopy();if(channel_)UnmapViewOfFile(channel_);channel_=nullptr;if(mapping_)CloseHandle(mapping_);mapping_=nullptr;}
public:
 explicit Client(uint32_t pid,uint64_t generation):pid_(pid),owner_(GetCurrentProcessId()),generation_(generation){}
 ~Client(){close();}Client(const Client&)=delete;Client&operator=(const Client&)=delete;
 Token acquire(uint32_t match){
  std::lock_guard<std::mutex>lock(mutex_);if(!open())return {};
  const uint32_t now=GetTickCount();heartbeat(now);
  for(int retry=0;retry<2;++retry){
   const LONG active=channel_->active;if(active<0||active>1){clearCopy();return {};}
   auto&s=channel_->slots[active];const LONG seq=s.seq;if(seq&1)continue;MemoryBarrier();const Key key=s.key;const uint32_t count=s.count;const DataRef ref=s.data;const Counters stats=s.counters;
   if(key.pid!=pid_||key.owner!=owner_||!key.match||key.frame<0||count>MaxPages||key.connectionLow!=uint32_t(generation_)||key.connectionHigh!=uint32_t(generation_>>32)||
      (count?(!dataCapacity(ref.capacity)||count>ref.capacity||!ref.allocation):(ref.capacity||ref.allocation)))return {};
   if(uint32_t(now-key.tick)>500){
    const LONG observedSeq=channel_->observationSeq;if(observedSeq&1)return {};MemoryBarrier();
    const Key observed=channel_->observed;const uint32_t tick=channel_->observedTick;MemoryBarrier();
    if(observedSeq!=channel_->observationSeq||uint32_t(now-tick)>500||!key.sameFrame(observed))return {};
   }
   if(match&&key.match!=match)return {};
   if(latest_&&copiedSerial_==key.serial&&latest_->key.sameFrame(key)&&latest_->pages.size()==count&&copiedSlot_==active&&sameData(copiedData_,ref)){
    MemoryBarrier();if(seq==s.seq&&active==channel_->active&&connectionMatches(channel_,uint32_t(generation_),uint32_t(generation_>>32)))return latest_;continue;
   }
   if(count&&!data_[active].open(key,uint32_t(active),ref))return {};
   auto next=std::make_shared<Snapshot>();const bool ok=next->import(key,stats,count?data_[active].pages():nullptr,count);
   MemoryBarrier();if(seq!=s.seq||active!=channel_->active||!connectionMatches(channel_,uint32_t(generation_),uint32_t(generation_>>32)))continue;if(!ok)return {};
   latest_=std::move(next);copiedSerial_=key.serial;copiedSlot_=active;copiedData_=ref;return latest_;
  }return {};
 }
 bool accepts(const Token&token){
  std::lock_guard<std::mutex>lock(mutex_);if(!token||!open()||channel_->active<0)return false;
  const LONG seq=channel_->observationSeq;if(seq&1)return false;MemoryBarrier();
  const Key observed=channel_->observed;const uint32_t tick=channel_->observedTick;MemoryBarrier();const uint32_t now=GetTickCount();
  return seq==channel_->observationSeq&&uint32_t(now-tick)<=1000&&token->key.sameWorld(observed)&&
   token->key.connectionLow==uint32_t(generation_)&&token->key.connectionHigh==uint32_t(generation_>>32)&&
   token->key.frame<=observed.frame&&(uint32_t(now-token->key.tick)<=500||token->key.frame==observed.frame);
 }
 void subscribe(uintptr_t reader,uint32_t demand){
  std::lock_guard<std::mutex>lock(mutex_);if(!open())return;const uint32_t now=GetTickCount();
  auto&lease=leases_[reader];lease.used|=demand|Base;for(unsigned bit=0;bit<6;++bit)if((demand|Base)&(1u<<bit))lease.ticks[bit]=now;
  heartbeat(now);
 }
 void request(const std::vector<uint32_t>&pages){
  std::lock_guard<std::mutex>lock(mutex_);if(!open())return;const uint32_t now=GetTickCount();channel_->heartbeat=now;refreshDemand(now);
  // Missing or newly used ranges become a demand for the NEXT capture. A
  // consumer must not quietly supplement the sealed frame through RPM.
  // Retire expired keys before admitting replacements at the capacity limit.
  pruneRequests(now);
  for(uint32_t p:pages)if(!(p&4095u)&&range(p,PageSize)){
   auto found=wanted_.find(p);if(found!=wanted_.end())found->second=now;
   else if(wanted_.size()<MaxPages){wanted_.emplace(p,now);wantedDirty_=true;}
  }
  // Unchanged membership only renews local leases; do not copy the entire
  // list or make Server allocate another vector on every high-rate read pass.
  publishRequests(now);
 }
};
inline std::shared_ptr<Client>acquireClient(uint32_t pid,uint64_t generation){
 static std::mutex mutex;static std::map<std::pair<uint32_t,uint64_t>,std::weak_ptr<Client>>clients;
 std::lock_guard<std::mutex>lock(mutex);auto&weak=clients[{pid,generation}];auto client=weak.lock();if(!client){client=std::make_shared<Client>(pid,generation);weak=client;}return client;
}

class Server {
 HANDLE mapping_=nullptr;Channel*channel_=nullptr;uint32_t owner_=0,openAt_=0;
 std::array<DataView,2>data_;
 std::vector<uint32_t>requests_;uint32_t requestRevision_=0;
 uint64_t connection_=0;
 bool clientAlive(uint32_t now)const{return channel_&&connectionMatches(channel_,uint32_t(connection_),uint32_t(connection_>>32))&&uint32_t(now-channel_->heartbeat)<=1000;}
 bool needed(uint32_t now){
  if(clientAlive(now))return true;
  requests_.clear();requestRevision_=0;
  if(channel_&&connectionMatches(channel_,uint32_t(connection_),uint32_t(connection_>>32))&&channel_->active>=0)retire();
  return false;
 }
 void close(){for(auto&view:data_)view.close();if(channel_)UnmapViewOfFile(channel_);channel_=nullptr;if(mapping_)CloseHandle(mapping_);mapping_=nullptr;owner_=0;connection_=0;requests_.clear();requestRevision_=0;}
 void failed(PublishError error){if(channel_){channel_->publishError=uint32_t(error);channel_->publishSystemError=(error==PublishError::CreateMapping||error==PublishError::MapView)?GetLastError():0;++channel_->publishFailures;}}
public:
 ~Server(){disconnect();}
 bool wanted(uint32_t owner,uint32_t now){
  if(owner_!=owner){retire();close();openAt_=0;owner_=owner;}
  if(!owner)return false;
  if(!channel_&&(!openAt_||uint32_t(now-openAt_)>=100)){
   openAt_=now;wchar_t path[100];name(path,GetCurrentProcessId(),owner);
   mapping_=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,path);
   if(mapping_)channel_=static_cast<Channel*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(Channel)));
   if(!valid(channel_,GetCurrentProcessId(),owner)){close();owner_=owner;}
  }
  if(!channel_)return false;
  const LONG seq=channel_->connectionSeq;if(seq&1)return false;MemoryBarrier();const uint64_t connection=uint64_t(channel_->connectionHigh)<<32|channel_->connectionLow;MemoryBarrier();if(seq!=channel_->connectionSeq)return false;
  if(connection!=connection_){retire();connection_=connection;requests_.clear();requestRevision_=0;}
  return needed(now);
 }
 const std::vector<uint32_t>&requests(){
  if(!needed(GetTickCount()))return requests_;
  const LONG seq=channel_->demandSeq;if(seq&1)return requests_;MemoryBarrier();
  const uint32_t count=channel_->demandCount,revision=channel_->demandRevision;
  if(revision==requestRevision_||count>MaxPages)return requests_;
  std::vector<uint32_t>next(count);if(count)std::memcpy(next.data(),channel_->demands,size_t(count)*4);
  MemoryBarrier();if(seq!=channel_->demandSeq)return requests_;
  for(uint32_t page:next)if((page&4095u)||!range(page,PageSize))return requests_;
  requests_=std::move(next);requestRevision_=revision;return requests_;
 }
 uint32_t demandRevision()const{return requestRevision_;}
 uint32_t demandMask()const{return clientAlive(GetTickCount())?uint32_t(channel_->demandMask):0;}
 uint64_t connection()const{return connection_;}
 void observe(const Key&key){if(!needed(GetTickCount()))return;InterlockedIncrement(&channel_->observationSeq);channel_->observed=key;channel_->observedTick=GetTickCount();MemoryBarrier();InterlockedIncrement(&channel_->observationSeq);}
 void publish(const Snapshot&frame){
  if(!needed(GetTickCount()))return;
  if(!frame.sealed()||frame.overflow()||frame.pages.size()>MaxPages){failed(PublishError::Capacity);return;}
  if(frame.key.pid!=GetCurrentProcessId()||frame.key.owner!=owner_||!connectionMatches(channel_,frame.key.connectionLow,frame.key.connectionHigh)){failed(PublishError::Identity);return;}
  // Same-frame wakeups may need to recover an expired transport, but an
  // already exported immutable sample must never be recopied on each wake.
  const LONG active=channel_->active;
  if(active>=0&&active<2){const auto&current=channel_->slots[active];
   if(!(current.seq&1)&&current.count==frame.pages.size()&&!std::memcmp(&current.key,&frame.key,sizeof(Key))&&
      (current.count?data_[active].matches(frame.key,uint32_t(active),current.data):(!current.data.capacity&&!current.data.allocation)))return;
  }
  const LONG slot=channel_->active==0?1:0;auto&out=channel_->slots[slot];
  const uint32_t count=uint32_t(frame.pages.size());DataView next;bool replace=false;
  if(count){
   const DataRef current=data_[slot].ref();replace=current.capacity<count||!data_[slot].matches(frame.key,uint32_t(slot),current);
   if(replace){const auto error=next.create(frame.key,uint32_t(slot),grownCapacity(count));if(error!=PublishError::None){failed(error);return;}}
  }
  // Allocation failure above leaves both the active descriptor and its pages
  // untouched. No published sample receives an invented newer timestamp.
  InterlockedIncrement(&out.seq);if(replace)data_[slot].swap(next);
  if(count)std::memcpy(data_[slot].pages(),frame.pages.data(),size_t(count)*sizeof(Page));
  out.key=frame.key;out.counters=frame.counters;out.count=count;out.data=count?data_[slot].ref():DataRef{};
  MemoryBarrier();InterlockedIncrement(&out.seq);
  if(!connectionMatches(channel_,frame.key.connectionLow,frame.key.connectionHigh)){failed(PublishError::Identity);return;}
  channel_->publishError=uint32_t(PublishError::None);channel_->publishSystemError=0;InterlockedExchange(&channel_->active,slot);
 }
 void retire(){if(channel_){InterlockedExchange(&channel_->active,-1);InterlockedIncrement(&channel_->observationSeq);channel_->observedTick=0;MemoryBarrier();InterlockedIncrement(&channel_->observationSeq);}for(auto&view:data_)view.close();}
 void disconnect(){retire();close();openAt_=0;}
};
}
