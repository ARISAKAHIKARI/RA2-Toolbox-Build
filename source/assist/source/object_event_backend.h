#pragma once
#include <stdint.h>
// Game's high-level TechnoClass::ClickedEvent(NetworkEvents).
// This is an experimental delivery backend, not an event constructor.
// YRpp TechnoClass.h and the user's two matching EXEs:
// Building vtable 0x7E3EBC + 0x374 -> 0x6FFE00.
namespace assist { namespace objectqueue {
enum class Backend : uint32_t { ConstructAndEnqueue=0, ClickedEvent=1 };
enum class Result : int32_t {
 Idle=0, Appended=1, DefaultEventClass=2,
 UnsupportedSession=-1, InvalidEvent=-2, SignatureMismatch=-3,
 HookConflict=-4, QueueFull=-5, NoAppend=-6, NotReady=-7,
 WrongThread=-8, InvalidActor=-9
};
constexpr uint32_t ClickedEventVtableOffset=0x374;
constexpr uint32_t ClickedEventVanilla=0x006FFE00;
struct Capability {
 bool multiplayer=false, validEvent=false, actorOwnedAlive=false,
      signature=false, unhooked=false, vtableValid=false,
      sameGameThread=false, ready=false, hasRoom=false;
};
inline Result admit(Backend mode,const Capability& c){
 if(mode==Backend::ConstructAndEnqueue)return Result::DefaultEventClass;
 if(!c.multiplayer)return Result::UnsupportedSession;
 if(!c.validEvent)return Result::InvalidEvent;
 if(!c.actorOwnedAlive)return Result::InvalidActor;
 if(!c.signature||!c.vtableValid)return Result::SignatureMismatch;
 if(!c.unhooked)return Result::HookConflict;
 if(!c.sameGameThread)return Result::WrongThread;
 if(!c.ready)return Result::NotReady;
 if(!c.hasRoom)return Result::QueueFull;
 return Result::Appended;
}
// ClickedEvent() may return true when the OutList is full. Confirmation
// requires a real ring-buffer append; a bool result alone is not evidence.
inline bool appended(int beforeCount,int beforeTail,int afterCount,int afterTail){
 return beforeCount>=0 && beforeCount<128 && beforeTail>=0 && beforeTail<128 &&
  afterCount==beforeCount+1 && afterTail==((beforeTail+1)&127);
}
}} // namespace assist::objectqueue
