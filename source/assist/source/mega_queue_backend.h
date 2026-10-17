#pragma once
#include <stdint.h>

// This is a DELIVERY backend, not a TickProvider. It operates only after
// common unit ownership, epoch, manual takeover and queue budget checks.
namespace assist { namespace megaqueue {
enum class Backend : uint32_t { ConstructAndEnqueue=0, GameQueueMegaMission=1 };
enum class Result : int32_t {
 Idle=0, Queued=1, DefaultEventClass=2,
 UnsupportedSession=-1, UnsupportedArguments=-2, SignatureMismatch=-3,
 ConflictingHook=-4, QueueFull=-5, EngineRejected=-6, NotReady=-7, NoAppend=-8
};
struct Capability {
 bool isMultiplayer=false, isSingleplayer=false, engineSignature=false,
      knownOriginalOrOwnedHook=false, timingCallbackExecutable=false;
};
// The engine implementation at 0x646E90 always writes the OutList;
// refuse to call it in SP (where our existing sender uses DoList).
inline Result admit(Backend mode,Capability cap,uint32_t nearest,uint8_t nearestTag){
 if(mode==Backend::ConstructAndEnqueue)return Result::DefaultEventClass;
 if(!cap.isMultiplayer||cap.isSingleplayer)return Result::UnsupportedSession;
 if(nearest||nearestTag)return Result::UnsupportedArguments; // no ABI path for extra nearest data
 if(!cap.engineSignature)return Result::SignatureMismatch;
 if(!cap.knownOriginalOrOwnedHook)return Result::ConflictingHook;
 if(!cap.timingCallbackExecutable)return Result::NotReady;
 return Result::Queued;
}
// Native bool is not sufficient to prove delivery if a hook changes
// semantics. Require the game OutList's actual one-slot advance.
inline bool appended(int beforeCount,int beforeTail,int afterCount,int afterTail){
 return beforeCount>=0&&beforeCount<128&&beforeTail>=0&&beforeTail<128&&
  afterCount==beforeCount+1&&afterTail==((beforeTail+1)&127);
}
struct Target { uint32_t value=0;uint8_t tag=0;uint8_t padding[3]{}; };
static_assert(sizeof(Target)==8,"fastcall reference must be padded to 8 bytes");
}} // namespace assist::megaqueue
