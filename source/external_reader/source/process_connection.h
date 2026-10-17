#pragma once
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
namespace external {
// Immutable identity. Each task holds a strong reference, so another module's
// detach cannot close its in-flight handle. This object has no mutable cache.
struct ProcessConnection {
 const HANDLE handle;
 const DWORD pid;
 const bool readable;
 const uint64_t generation;
 ProcessConnection(HANDLE h,DWORD p,bool r,uint64_t g):handle(h),pid(p),readable(r),generation(g){}
 ProcessConnection(const ProcessConnection&)=delete;
 ProcessConnection& operator=(const ProcessConnection&)=delete;
 ~ProcessConnection(){CloseHandle(handle);}
 bool alive()const{DWORD code=0;return GetExitCodeProcess(handle,&code)&&code==STILL_ACTIVE;}
};
struct ConnectionHub {
 std::mutex mutex;
 std::unordered_map<uint64_t,std::weak_ptr<const ProcessConnection>> connections;
 uint64_t nextGeneration=0;
};
inline ConnectionHub& connectionHub(){static ConnectionHub hub;return hub;}
inline std::shared_ptr<const ProcessConnection> acquireConnection(DWORD pid,bool readable,DWORD& error){
 auto& hub=connectionHub();
 // Only registry/connection creation is serialized. No sampling, cache access,
 // ReadProcessMemory or snapshot construction takes this mutex.
 std::lock_guard<std::mutex> guard(hub.mutex);
 error=0;
 for(auto i=hub.connections.begin();i!=hub.connections.end();)
  if(i->second.expired())i=hub.connections.erase(i);else ++i;
 const uint64_t key=(uint64_t(pid)<<1)|uint64_t(readable);
 auto& slot=hub.connections[key];
 if(auto existing=slot.lock();existing&&existing->alive())return existing;
 HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|(readable?PROCESS_VM_READ:0),FALSE,pid);
 if(!h){error=GetLastError();hub.connections.erase(key);return {};}
 auto result=std::make_shared<ProcessConnection>(h,pid,readable,++hub.nextGeneration);
 if(!result->alive()){error=ERROR_INVALID_HANDLE;hub.connections.erase(key);return {};}
 slot=result;return result;
}
}
