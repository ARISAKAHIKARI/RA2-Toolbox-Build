#pragma once
#include <cstdint>
// YRpp d9795546: Unsorted::UserInputLocked; TriggerAction 46/47.
// This is the local UI lock, not House ownership or defeated-player state.
namespace assist::inputlock {
constexpr uint32_t Address=0xA8ED9C;
enum Status { Off=0,Watching=1,Unlocked=2,Waiting=3,Unreadable=-1,WriteFailed=-2 };
template<class Read,class Write>int maintain(bool enabled,bool playable,Read&&read,Write&&write){
 if(!enabled)return Off;if(!playable)return Waiting;
 uint8_t locked=0;if(!read(Address,&locked,1)||locked>1)return Unreadable;
 if(!locked)return Watching;
 const uint8_t clear=0;if(!write(Address,&clear,1))return WriteFailed;
 if(!read(Address,&locked,1)||locked)return WriteFailed;
 return Unlocked;
}
}
