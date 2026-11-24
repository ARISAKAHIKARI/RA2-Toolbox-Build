#pragma once
#include <cstdint>

namespace assist { namespace crcshadowsingle {

enum class Result : uint8_t {
 Idle, Armed, Busy, NativeStore, SuppressStore, IdentityChanged, FaultSuppressed
};

// A single synchronous Map::Reveal call may suppress only the MapIsClear store
// for the exact House whose identity was checked before entering that call.
// Proposed x86 leaf contract after the guarded callback:
//   pushfd; cmp byte ptr [crcMapSkipStore],0; jne skip_original_store
//   mov byte ptr [ecx+241h],bl
// skip_original_store: mov byte ptr [crcMapSkipStore],0; popfd
// The callback sets the one-shot flag only for Scope::house. On callback SEH,
// the guarded wrapper passes captured ECX to abort(), which arms the same
// target-only skip. The branch's CMP flags are discarded by POPFD; MOV itself
// preserves flags and all original GPRs remain owned by the existing callback.
// The actual naked trampoline and Win32 SEH wiring are not implemented here.
struct Scope {
 uint32_t epoch=0,house=0,houseID=0;
 bool active=false,skipStore=false,faulted=false;
 unsigned suppressed=0;
};

inline Result begin(Scope&scope,uint32_t epoch,uint32_t house,uint32_t houseID){
 if(scope.active||scope.skipStore)return Result::Busy;
 if(!epoch||house<0x10000||!houseID)return Result::Idle;
 scope={epoch,house,houseID,true,false,false,0};
 return Result::Armed;
}

inline Result beforeStore(Scope&scope,uint32_t epoch,uint32_t currentHouse,uint32_t currentHouseID){
 if(!scope.active)return Result::NativeStore;
 // A nested/native reveal for another House must remain entirely native.
 if(currentHouse!=scope.house)return Result::NativeStore;
 // Same address but changed epoch/identity means pointer reuse or world change.
 // Do not suppress a write into a different object.
 if(epoch!=scope.epoch||currentHouseID!=scope.houseID){
  scope.faulted=true;return Result::IdentityChanged;
 }
 scope.skipStore=true;
 ++scope.suppressed;
 return Result::SuppressStore;
}

// Used by the SEH exception handler. The caller passes the ECX House captured
// before entering the guarded callback; only the scoped target may be skipped.
inline bool abort(Scope&scope,uint32_t currentHouse){
 if(!scope.active||currentHouse!=scope.house)return false;
 scope.skipStore=true;scope.faulted=true;return true;
}

inline bool consumeSkip(Scope&scope){
 const bool skip=scope.skipStore;
 scope.skipStore=false;
 return skip;
}

inline void end(Scope&scope){
 scope.active=false;scope.skipStore=false;
}

}}
