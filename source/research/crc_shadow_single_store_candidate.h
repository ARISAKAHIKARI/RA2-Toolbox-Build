#pragma once
#include <cstdint>

namespace assist { namespace crcshadowsingle {

enum class Result : uint8_t { Idle, Armed, Busy, SuppressStore, NativeStore, FaultSuppressed, Ended };

// Intended naked-leaf suffix after the guarded callback (research contract):
//   pushfd
//   cmp byte ptr [crcMapSkipStore],0
//   jne skip_original_store
//   mov byte ptr [ecx+241h],bl
// skip_original_store:
//   mov byte ptr [crcMapSkipStore],0
//   popfd
// The branch preserves the original MOV semantics when inactive, changes no
// GPR, and restores EFLAGS because CMP itself changes flags.
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

// Called only at the single Map::Reveal +0x241 store site. Every store reached
// while this synchronous tool-owned scope is active is tool-originated; natural
// calls outside the scope execute the original instruction unchanged.
inline Result beforeStore(Scope&scope,bool houseIdentityValid){
 if(!scope.active)return Result::NativeStore;
 scope.skipStore=true;
 ++scope.suppressed;
 if(!houseIdentityValid){scope.faulted=true;return Result::FaultSuppressed;}
 return Result::SuppressStore;
}

// The naked trampoline consumes this flag immediately after the callback.
// It must not alter any GPR or EFLAGS while deciding whether to execute MOV.
inline bool consumeSkip(Scope&scope){
 const bool skip=scope.skipStore;
 scope.skipStore=false;
 return skip;
}

// A callback fault inside the guarded leaf still has to suppress the tool
// store. Leave active set until the outer RuntimeScopedCall finally runs End.
inline void abort(Scope&scope){
 if(scope.active){scope.skipStore=true;scope.faulted=true;}
}

inline void end(Scope&scope){
 scope.active=false;
 scope.skipStore=false;
}

}}
