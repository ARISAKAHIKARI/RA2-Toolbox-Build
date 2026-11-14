#pragma once
#if defined(RA2_UNIFIED)
#error Game execution entry is Runtime-only; host modules must submit through host_game_input.
#endif
#include <windows.h>
#include <atomic>
#include "game_entry_validation.h"
#include "game_entry_api.h"
#include "runtime_seh_scope.h"
namespace gameentry {
using Manage=int32_t (__stdcall*)(Request*);
// All game components are linked into GameRuntime.dll. There is no runtime
// DLL lookup, LoadLibrary fallback or per-DLL manager instance.
extern "C" int32_t __stdcall ManageGameEntry(Request*);
inline Manage manager(){return &ManageGameEntry;}
inline uint32_t moduleIdentity(){
 // A module's base is immutable for its lifetime. Hot draw callbacks used to
 // enter the loader on every Capture/Validate/Leave operation.
 static std::atomic<uint32_t>cached{0};
 const uint32_t found=cached.load(std::memory_order_acquire);
 if(found)return found;
 HMODULE m=nullptr;
 GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
   reinterpret_cast<LPCWSTR>(&moduleIdentity),&m);
 const uint32_t identity=uint32_t(uintptr_t(m));
 if(identity){uint32_t empty=0;cached.compare_exchange_strong(empty,identity,std::memory_order_release,std::memory_order_relaxed);}
 return identity;
}
inline bool manage(Request&r){auto f=manager();return f&&f(&r)==int32_t(Result::Ok);}
inline std::atomic<uint32_t>activeToken{0};
static_assert(std::atomic<uint32_t>::is_always_lock_free,"Runtime token reads must be lock-free");
inline uint32_t currentToken(){return activeToken.load(std::memory_order_acquire);}
inline bool enter(Client client,bool bind,Request&r){
 r={};r.operation=Operation::Enter;r.client=client;r.module=moduleIdentity();r.bindThread=bind;
 if(!manage(r))return false;
 activeToken.store(r.token,std::memory_order_release);return true;
}
inline bool capture(Client client,Request&r){
 r={};r.operation=Operation::Capture;r.client=client;r.module=moduleIdentity();
 r.token=currentToken();
 if(!manage(r))return false;
 activeToken.store(r.token,std::memory_order_release);return true;
}
inline void leave(Request&r){r.operation=Operation::Leave;
 if(manage(r))activeToken.store(r.token,std::memory_order_release);
}
inline bool active(Client c){
 const uint32_t token=currentToken();
 return token&&validateCurrent(c,moduleIdentity(),token);
}
struct Scope {
 Request request{};bool acquired=false;
 Scope(Client c,bool bind=false):acquired(enter(c,bind,request)){}
 void release(){if(acquired){acquired=false;leave(request);}}
 ~Scope(){release();}
 explicit operator bool()const{return acquired;}
 Scope(const Scope&)=delete;Scope&operator=(const Scope&)=delete;
};
// Own the exact Enter lease outside the SEH callback frame. The same release
// runs after an ordinary return or a platform-owned exception unwind.
template<class Fn>bool scoped(Client client,bool bind,Fn&& fn){
 Scope entry(client,bind);if(!entry)return false;
 runtimeunwind::run([&]{fn();},[&]{entry.release();});return true;
}
inline bool patch(Client c,uint32_t at,const uint8_t*before,const uint8_t*after,unsigned n){
 if(!before||!after||n>16||!n)return false;
 Request r{};r.operation=Operation::Patch;r.client=c;r.module=moduleIdentity();r.address=at;r.length=n;r.token=currentToken();
 for(unsigned i=0;i<n;++i){r.before[i]=before[i];r.after[i]=after[i];}return manage(r);
}
inline Request inspect(uint32_t at=0,bool observe=false){
 // Inspect-only opt-in: bindThread requests diagnostics, never a thread bind.
 Request r{};r.client=Client::Assist;r.module=moduleIdentity();r.address=at;r.bindThread=observe;manage(r);return r;
}
inline void markConflict(uint32_t at){Request r{};r.operation=Operation::MarkConflict;
 r.client=Client::Assist;r.module=moduleIdentity();r.address=at;r.token=currentToken();manage(r);}
}
