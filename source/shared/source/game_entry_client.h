#pragma once
#if defined(RA2_UNIFIED)
#error Game execution entry is Runtime-only; host modules must submit through host_game_input.
#endif
#include <windows.h>
#include "game_entry_api.h"
namespace gameentry {
using Manage=int32_t (__stdcall*)(Request*);
// All game components are linked into GameRuntime.dll. There is no runtime
// DLL lookup, LoadLibrary fallback or per-DLL manager instance.
extern "C" int32_t __stdcall ManageGameEntry(Request*);
inline Manage manager(){return &ManageGameEntry;}
inline uint32_t moduleIdentity(){
 // A module's base is immutable for its lifetime. Hot draw callbacks used to
 // enter the loader on every Capture/Validate/Leave operation.
 static volatile LONG cached=0;
 const uint32_t found=uint32_t(InterlockedCompareExchange(&cached,0,0));
 if(found)return found;
 HMODULE m=nullptr;
 GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
   reinterpret_cast<LPCWSTR>(&moduleIdentity),&m);
 const uint32_t identity=uint32_t(uintptr_t(m));
 if(identity)InterlockedCompareExchange(&cached,LONG(identity),0);
 return identity;
}
inline bool manage(Request&r){auto f=manager();return f&&f(&r)==int32_t(Result::Ok);}
inline volatile LONG activeToken=0;
inline bool enter(Client client,bool bind,Request&r){
 r={};r.operation=Operation::Enter;r.client=client;r.module=moduleIdentity();r.bindThread=bind;
 if(!manage(r))return false;
 InterlockedExchange(&activeToken,LONG(r.token));return true;
}
inline bool capture(Client client,Request&r){
 r={};r.operation=Operation::Capture;r.client=client;r.module=moduleIdentity();
 r.token=uint32_t(InterlockedCompareExchange(&activeToken,0,0));
 if(!manage(r))return false;
 InterlockedExchange(&activeToken,LONG(r.token));return true;
}
inline void leave(Request&r){r.operation=Operation::Leave;
 if(manage(r))InterlockedExchange(&activeToken,LONG(r.token));
}
inline bool active(Client c){
 Request r{};r.operation=Operation::Validate;r.client=c;r.module=moduleIdentity();
 r.token=uint32_t(InterlockedCompareExchange(&activeToken,0,0));return r.token&&manage(r);
}
struct Scope {
 Request request{};bool acquired=false;
 Scope(Client c,bool bind=false):acquired(enter(c,bind,request)){}
 ~Scope(){if(acquired)leave(request);}
 explicit operator bool()const{return acquired;}
 Scope(const Scope&)=delete;Scope&operator=(const Scope&)=delete;
};
inline bool patch(Client c,uint32_t at,const uint8_t*before,const uint8_t*after,unsigned n){
 if(!before||!after||n>16||!n)return false;
 Request r{};r.operation=Operation::Patch;r.client=c;r.module=moduleIdentity();r.address=at;r.length=n;r.token=uint32_t(InterlockedCompareExchange(&activeToken,0,0));
 for(unsigned i=0;i<n;++i){r.before[i]=before[i];r.after[i]=after[i];}return manage(r);
}
inline Request inspect(uint32_t at=0){
 Request r{};r.client=Client::Assist;r.module=moduleIdentity();r.address=at;manage(r);return r;
}
inline void markConflict(uint32_t at){Request r{};r.operation=Operation::MarkConflict;
 r.client=Client::Assist;r.module=moduleIdentity();r.address=at;r.token=uint32_t(InterlockedCompareExchange(&activeToken,0,0));manage(r);}
}
