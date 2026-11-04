#pragma once
// Include before defining the Win32 profile API aliases in a HOST translation
// unit. GameRuntime and the x86 hook broker do not use this service.
#ifdef WritePrivateProfileStringW
#pragma push_macro("WritePrivateProfileStringW")
#undef WritePrivateProfileStringW
#define ASYNCINI_RESTORE_WRITE 1
#endif
#ifdef GetPrivateProfileStringW
#pragma push_macro("GetPrivateProfileStringW")
#undef GetPrivateProfileStringW
#define ASYNCINI_RESTORE_READ 1
#endif
#ifdef GetPrivateProfileIntW
#pragma push_macro("GetPrivateProfileIntW")
#undef GetPrivateProfileIntW
#define ASYNCINI_RESTORE_INT 1
#endif
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <cwctype>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace asyncini {
namespace core {
constexpr unsigned InvalidArgument=87,QueueFull=8,Stopped=995,WriteFault=29;
enum class Kind { Set,DeleteKey,DeleteSection,Flush,Native };
struct Text {
 bool null=true;std::wstring value;
 Text()=default;explicit Text(const wchar_t* p):null(!p),value(p?p:L""){}
 const wchar_t* pointer()const{return null?nullptr:value.c_str();}
};
struct Operation {
 Text section,key,value,path;Kind kind=Kind::Native;
 uint64_t revision=0,firstRevision=0;
 Operation()=default;
 Operation(const wchar_t*s,const wchar_t*k,const wchar_t*v,const wchar_t*p):section(s),key(k),value(v),path(p){
  if(!s&&!k&&!v)kind=Kind::Flush;
  else if(s&&!k)kind=Kind::DeleteSection;
  else if(s&&k&&!v)kind=Kind::DeleteKey;
  else if(s&&k&&v)kind=Kind::Set;
 }
 bool barrier()const{return kind!=Kind::Set;}
};
struct Result {bool ok=true;unsigned error=0;};
inline Result nativeResult(Kind kind,bool returnedSuccess,unsigned lastError){
 // Three NULL arguments refresh profile/registry-mapping cache state. This
 // is an ordered management barrier, not a file durability operation. The
 // API documents a zero return for a cache flush; even a pre-cleared
 // LastError can report an internal missing mapping (observed on Windows).
 // Neither value is a separate success predicate for this management call.
 // Actual Set/Delete results remain authoritative and a barrier never clears
 // their failures. Ordinary writes/deletes still reject zero + no error.
 // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-writeprivateprofilestringw
 if(kind==Kind::Flush||returnedSuccess)return{true,0};
 return{false,lastError?lastError:WriteFault};
}
struct Status {
 size_t pending=0;bool busy=false,failed=false,accepting=true,stopped=false;
 unsigned errorCode=0;std::wstring errorPath;
 // completedRevision is the processed low-water mark, including superseded
 // or failed operations. Successful writes additionally require !failed;
 // these counters do not claim power-loss or FlushFileBuffers durability.
 uint64_t submittedRevision=0,completedRevision=0,errorRevision=0;
};
struct Lookup {bool known=false,present=false;std::wstring value;};
inline std::wstring folded(const std::wstring& s,bool path=false){
 std::wstring out=s;
#ifdef _WIN32
 if(!out.empty()){
  const int n=LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,out.data(),int(out.size()),nullptr,0,nullptr,nullptr,0);
  if(n>0){std::wstring mapped(size_t(n),L'\0');if(LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,out.data(),int(out.size()),mapped.data(),n,nullptr,nullptr,0)>0)out.swap(mapped);}
 }
#else
 for(auto& c:out)c=wchar_t(std::towlower(c));
#endif
 if(path)for(auto&c:out)if(c==L'/')c=L'\\';
 return out;
}
struct Key {
 std::wstring path,section,key;
 bool operator<(const Key&b)const{if(path!=b.path)return path<b.path;if(section!=b.section)return section<b.section;return key<b.key;}
 bool operator==(const Key&b)const{return path==b.path&&section==b.section&&key==b.key;}
};
inline Key keyOf(const Operation&o){return{folded(o.path.value,true),folded(o.section.value),folded(o.key.value)};}
inline bool cacheable(const Operation&o){return !o.path.null&&!o.section.null&&(o.kind==Kind::Set||o.kind==Kind::DeleteKey||o.kind==Kind::DeleteSection);}
inline Key sectionKey(Key k){k.key.clear();return k;}
struct Cached {Operation op;bool failed=false,pending=true;};
struct Failure {Operation op;unsigned code=0;};
// One owned queue, one persistent writer. No disk I/O or waiting for the writer
// occurs in enqueue(), lookup(), status(), requestFlush() or beginShutdown().
class Store {
public:
 using Writer=std::function<Result(const Operation&)>;
 using Notify=std::function<void()>;
private:
 Writer writer_;size_t capacity_,cacheCapacity_;
 mutable std::mutex mutex_;std::condition_variable wake_;
 std::deque<Operation> queue_;std::thread worker_;
 std::map<Key,Cached> values_,sections_;
 std::map<std::wstring,Failure> failures_;
 uint64_t submitted_=0;bool busy_=false,accepting_=true,stopped_=false;
 Operation executing_;
 // Unsubscribe synchronizes with a short notification already in flight.
 // The callback must only enqueue/post work; it must not call setNotify().
 struct Notice {
  bool working=false,failed=false,accepting=true,stopped=false;
  unsigned code=0;std::wstring path;
  bool operator==(const Notice&b)const{return working==b.working&&failed==b.failed&&accepting==b.accepting&&stopped==b.stopped&&code==b.code&&path==b.path;}
 };
 std::mutex notifyMutex_;Notify notify_;Notice lastNotice_;bool noticeValid_=false;
 static std::wstring failureKey(const Operation&o){
  auto k=keyOf(o);return std::to_wstring(int(o.kind))+L":"+(o.path.null?L"N":L"V")+(o.section.null?L"N":L"V")+(o.key.null?L"N":L"V")+L"\n"+std::to_wstring(k.path.size())+L":"+k.path+L"\n"+std::to_wstring(k.section.size())+L":"+k.section+L"\n"+k.key;
 }
 void notify(){
  std::lock_guard<std::mutex> l(notifyMutex_);if(!notify_)return;
  const auto s=status();Notice now{bool(s.pending||s.busy),s.failed,s.accepting,s.stopped,s.errorCode,s.errorPath};
  // A whole-config save may submit thousands of keys. The visible console
  // needs edges (saving/idle/error), not one posted message per INI key.
  if(noticeValid_&&now==lastNotice_)return;
  lastNotice_=std::move(now);noticeValid_=true;notify_();
 }
 void failLocked(const Operation&o,unsigned code){
  auto id=failureKey(o);
  const auto prior=failures_.find(id);
  // A writer already in flight can fail after a newer submission has been
  // rejected. Preserve the newer desired operation, not that older result.
  if(prior!=failures_.end()&&prior->second.op.revision>o.revision)return;
  if(failures_.size()>=cacheCapacity_&&!failures_.count(id))failures_.erase(failures_.begin());
  failures_[std::move(id)]={o,code?code:WriteFault};
 }
 bool canCacheLocked(const Operation&o)const{
  if(!cacheable(o))return true;
  const auto k=keyOf(o);const auto&map=o.kind==Kind::DeleteSection?sections_:values_;
  return map.count(o.kind==Kind::DeleteSection?sectionKey(k):k)||values_.size()+sections_.size()<cacheCapacity_;
 }
 void cacheLocked(const Operation&o){
  if(!cacheable(o))return;
  const auto k=keyOf(o);
  if(o.kind==Kind::DeleteSection)sections_[sectionKey(k)]={o,false,true};
  else values_[k]={o,false,true};
 }
 bool appendLocked(Operation o,bool allowCoalesce=true){
  // Admission failures also own a revision. Otherwise completion of an older
  // in-flight write could clear or overwrite the newer rejected intent.
  o.revision=++submitted_;o.firstRevision=o.revision;
  if(!accepting_){failLocked(o,Stopped);return false;}
  if(!canCacheLocked(o)){failLocked(o,QueueFull);return false;}
  auto replace=queue_.end();
  if(allowCoalesce&&o.kind==Kind::Set){
   const auto k=keyOf(o);
   for(auto it=queue_.end();it!=queue_.begin();){--it;if(it->barrier())break;if(keyOf(*it)==k){replace=it;break;}}
  }
  if(replace==queue_.end()&&queue_.size()>=capacity_){
   // Rejected is not acknowledged as queued, but the bounded desired-value
   // cache must still own it so an explicit retry can persist the setting.
   cacheLocked(o);completeCacheLocked(o,false);failLocked(o,QueueFull);return false;
  }
  if(replace!=queue_.end())o.firstRevision=replace->firstRevision;
  cacheLocked(o);
  if(replace!=queue_.end())*replace=std::move(o);else queue_.push_back(std::move(o));
  if(!worker_.joinable())worker_=std::thread([this]{run();});
  wake_.notify_one();return true;
 }
 void clearMatchingFailuresLocked(const Operation&o){
  auto k=keyOf(o);
  for(auto it=failures_.begin();it!=failures_.end();){
   const auto&prior=it->second.op;auto p=keyOf(prior);
   const bool samePath=k.path==p.path&&o.path.null==prior.path.null;
   const bool sameKey=samePath&&k.section==p.section&&k.key==p.key&&
       (o.kind==Kind::Set||o.kind==Kind::DeleteKey)&&(prior.kind==Kind::Set||prior.kind==Kind::DeleteKey);
   const bool deletedSection=samePath&&k.section==p.section&&o.kind==Kind::DeleteSection&&prior.kind!=Kind::Flush;
   const bool sameBarrier=failureKey(o)==it->first;
   if((sameKey||deletedSection||sameBarrier)&&prior.revision<=o.revision)it=failures_.erase(it);else ++it;
  }
 }
 void completeCacheLocked(const Operation&o,bool ok){
  if(!cacheable(o))return;
  auto k=keyOf(o);
  if(o.kind==Kind::DeleteSection){
   auto it=sections_.find(sectionKey(k));
   if(it!=sections_.end()&&it->second.op.revision==o.revision){
    if(ok)sections_.erase(it);else{it->second.pending=false;it->second.failed=true;}
   }
   if(ok){
    for(auto v=values_.begin();v!=values_.end();){
     if(v->first.path==k.path&&v->first.section==k.section&&
        (v->second.op.revision<=o.revision||(!v->second.pending&&!v->second.failed)))v=values_.erase(v);else ++v;
    }
   }
  }else{
   auto it=values_.find(k);if(it==values_.end()||it->second.op.revision!=o.revision)return;
   const auto section=sections_.find(sectionKey(k));
   const bool compensate=section!=sections_.end()&&section->second.failed&&section->second.op.revision<o.revision;
   if(ok&&!compensate)values_.erase(it);else{it->second.pending=false;it->second.failed=!ok;}
  }
 }
 void run(){
  for(;;){
   Operation op;
   {std::unique_lock<std::mutex> l(mutex_);wake_.wait(l,[this]{return !queue_.empty()||!accepting_;});
    if(queue_.empty()){stopped_=true;break;}
    op=std::move(queue_.front());queue_.pop_front();executing_=op;busy_=true;
   }
   notify();const auto result=writer_(op);
   {std::lock_guard<std::mutex> l(mutex_);
    if(result.ok)clearMatchingFailuresLocked(op);else failLocked(op,result.error);
    completeCacheLocked(op,result.ok);busy_=false;executing_={};
   }
   notify();
  }
  notify();
 }
public:
 explicit Store(Writer writer,size_t capacity=4096,size_t cacheCapacity=8192):writer_(std::move(writer)),capacity_(std::max(size_t(1),capacity)),cacheCapacity_(std::max(capacity_+1,cacheCapacity)){}
 Store(const Store&)=delete;Store&operator=(const Store&)=delete;
 // Local stores in tests/worker-owned contexts may join on destruction. The
 // process singleton below is intentionally lifetime-long; UI shutdown polls.
 ~Store(){beginShutdown();setNotify({});if(worker_.joinable())worker_.join();}
 bool enqueue(Operation o){bool ok;{std::lock_guard<std::mutex>l(mutex_);ok=appendLocked(std::move(o));}notify();return ok;}
 bool write(const wchar_t*s,const wchar_t*k,const wchar_t*v,const wchar_t*p){return enqueue(Operation(s,k,v,p));}
 // Queue an ordered profile-cache refresh after preceding API calls. This
 // preserves the legacy NULL-argument behavior, not a file-buffer guarantee.
 bool requestFlush(const wchar_t*path=nullptr){return write(nullptr,nullptr,nullptr,path);}
 void setNotify(Notify fn){
  {std::lock_guard<std::mutex>l(notifyMutex_);notify_=std::move(fn);noticeValid_=false;}
  notify();
 }
 Status status()const{
  std::lock_guard<std::mutex>l(mutex_);Status s;s.pending=queue_.size();s.busy=busy_;s.accepting=accepting_;s.stopped=stopped_;s.failed=!failures_.empty();s.submittedRevision=submitted_;
  uint64_t first=submitted_+1;if(busy_)first=executing_.firstRevision;
  for(const auto&o:queue_)first=std::min(first,o.firstRevision);
  s.completedRevision=first-1;
  for(const auto&e:failures_)if(!s.errorCode||e.second.op.revision>=s.errorRevision){s.errorCode=e.second.code;s.errorRevision=e.second.op.revision;s.errorPath=e.second.op.path.value;}
  return s;
 }
 bool idle()const{const auto s=status();return !s.pending&&!s.busy;}
 void beginShutdown(){
  {std::lock_guard<std::mutex>l(mutex_);accepting_=false;if(!worker_.joinable())stopped_=true;wake_.notify_one();}
  notify();
 }
 Lookup lookup(const wchar_t*s,const wchar_t*k,const wchar_t*p)const{
  if(!s||!k||!p)return{};
  Operation query(s,k,L"",p);const auto key=keyOf(query);
  std::lock_guard<std::mutex>l(mutex_);const auto v=values_.find(key),section=sections_.find(sectionKey(key));
  if(section!=sections_.end()&&(v==values_.end()||section->second.op.revision>=v->second.op.revision))return{true,false,{}};
  if(v==values_.end())return{};
  return{true,v->second.op.kind==Kind::Set,v->second.op.value.value};
 }
 // An explicit user acknowledgement clears the reported error only, not the
 // owned desired values. A subsequent retry may still persist those values.
 void acknowledgeError(){
  {std::lock_guard<std::mutex>l(mutex_);failures_.clear();}notify();
 }
 // Replay the current owned desired state in revision order. This includes
 // post-delete compensating keys, so retrying a failed section deletion never
 // erases a newer successful setting. Unknown INI entries are never rewritten.
 bool retryFailed(const wchar_t*path=nullptr){
  bool ok=true;
  {std::lock_guard<std::mutex>l(mutex_);
   if(!accepting_)return false;
   const auto wanted=path?folded(path,true):std::wstring{};std::vector<Operation> replay;
   auto match=[&](const Operation&o){return !path||(!o.path.null&&folded(o.path.value,true)==wanted);};
   for(const auto&e:sections_)if(match(e.second.op))replay.push_back(e.second.op);
   for(const auto&e:values_){if(!match(e.second.op))continue;auto s=sections_.find(sectionKey(e.first));if(s==sections_.end()||e.second.op.revision>s->second.op.revision)replay.push_back(e.second.op);}
   for(const auto&e:failures_){
    const auto&o=e.second.op;if(!match(o))continue;
    if(!cacheable(o)){replay.push_back(o);continue;}
    const auto k=keyOf(o);const auto value=values_.find(k),section=sections_.find(sectionKey(k));
    const bool owned=(o.kind==Kind::DeleteSection?section!=sections_.end():value!=values_.end())||
      (section!=sections_.end()&&section->second.op.revision>=o.revision);
    // Cache-capacity admission may retain the failure without a value slot.
    // A later successful same-key operation removes this failure; otherwise
    // a missing Set/DeleteKey remains safe to replay in revision order.
    if(!owned&&(o.kind==Kind::Set||o.kind==Kind::DeleteKey))replay.push_back(o);
   }
   std::stable_sort(replay.begin(),replay.end(),[](const Operation&a,const Operation&b){return a.revision<b.revision;});
   if(replay.size()>capacity_-std::min(capacity_,queue_.size()))return false;
   for(auto&o:replay)if(!appendLocked(std::move(o),false)){ok=false;break;}
  }
  notify();return ok;
 }
};
inline std::wstring readValue(std::wstring value){
 if(value.size()>=2&&((value.front()==L'"'&&value.back()==L'"')||(value.front()==L'\''&&value.back()==L'\'')))value=value.substr(1,value.size()-2);
 return value;
}
inline uint32_t readInteger(const std::wstring& input,uint32_t fallback){
 const auto value=readValue(input);if(value.empty())return fallback;size_t p=0;while(p<value.size()&&std::iswspace(value[p]))++p;
 bool negative=false;if(p<value.size()&&(value[p]==L'+'||value[p]==L'-'))negative=value[p++]==L'-';
 unsigned base=10;if(p+1<value.size()&&value[p]==L'0'){auto c=value[p+1];if(c==L'x'||c==L'X'){base=16;p+=2;}else if(c==L'o'||c==L'O'){base=8;p+=2;}else if(c==L'b'||c==L'B'){base=2;p+=2;}}
 uint32_t number=0;for(;p<value.size();++p){auto c=value[p];unsigned digit=c>=L'0'&&c<=L'9'?unsigned(c-L'0'):c>=L'a'&&c<=L'f'?unsigned(c-L'a')+10:c>=L'A'&&c<=L'F'?unsigned(c-L'A')+10:99;if(digit>=base)break;number=number*base+digit;}
 return negative?uint32_t(0)-number:number;
}
}

#ifdef _WIN32
// Captured while the public aliases are undefined; calling the writer can
// never recurse through a host translation unit's API interception macros.
inline const auto nativeWrite=&::WritePrivateProfileStringW;
inline const auto nativeRead=&::GetPrivateProfileStringW;
inline const auto nativeInt=&::GetPrivateProfileIntW;
inline core::Store& store(){
 static auto* instance=new core::Store([](const core::Operation&o){
  SetLastError(ERROR_SUCCESS);const auto returned=nativeWrite(o.section.pointer(),o.key.pointer(),o.value.pointer(),o.path.pointer())!=FALSE;
  const auto code=returned?ERROR_SUCCESS:GetLastError();return core::nativeResult(o.kind,returned,unsigned(code));
 });return *instance;
}
inline BOOL write(LPCWSTR section,LPCWSTR key,LPCWSTR value,LPCWSTR path){
 if(store().write(section,key,value,path))return TRUE;
 SetLastError(store().status().errorCode);return FALSE;
}
inline core::Status status(){return store().status();}
inline bool idle(){return store().idle();}
inline bool requestFlush(LPCWSTR path=nullptr){return store().requestFlush(path);}
inline bool retryFailed(LPCWSTR path=nullptr){return store().retryFailed(path);}
inline void beginShutdown(){store().beginShutdown();}
inline void acknowledgeError(){store().acknowledgeError();}
inline void setNotify(core::Store::Notify fn){store().setNotify(std::move(fn));}
inline DWORD cachedReadString(LPCWSTR section,LPCWSTR key,LPCWSTR fallback,LPWSTR out,DWORD size,LPCWSTR path){
 if(!out||!size)return 0;const auto cached=store().lookup(section,key,path);
 if(!cached.known)return nativeRead(section,key,fallback,out,size,path);
 auto text=cached.present?core::readValue(cached.value):std::wstring(fallback?fallback:L"");
 if(!cached.present)while(!text.empty()&&text.back()==L' ')text.pop_back();
 const auto length=std::min<size_t>(text.size(),size_t(size)-1);std::copy_n(text.data(),length,out);out[length]=L'\0';return DWORD(length);
}
inline UINT cachedReadInt(LPCWSTR section,LPCWSTR key,INT fallback,LPCWSTR path){
 const auto cached=store().lookup(section,key,path);if(!cached.known)return nativeInt(section,key,fallback,path);
 return cached.present?UINT(core::readInteger(cached.value,uint32_t(fallback))):UINT(fallback);
}
#endif
}

#ifdef ASYNCINI_RESTORE_WRITE
#pragma pop_macro("WritePrivateProfileStringW")
#undef ASYNCINI_RESTORE_WRITE
#endif
#ifdef ASYNCINI_RESTORE_READ
#pragma pop_macro("GetPrivateProfileStringW")
#undef ASYNCINI_RESTORE_READ
#endif
#ifdef ASYNCINI_RESTORE_INT
#pragma pop_macro("GetPrivateProfileIntW")
#undef ASYNCINI_RESTORE_INT
#endif
