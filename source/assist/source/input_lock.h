#pragma once
#include <cstdint>
namespace assist::inputlock {
constexpr uint32_t Address=0xA8ED9C,Scenario=0xA8B230,ScenarioLock=0x35A2,Unlock=0x684290;
enum Status { Off=0,Watching=1,Unlocked=2,Waiting=3,Unreadable=-1,WriteFailed=-2 };
template<class Read,class UnlockNative>int maintain(bool enabled,bool playable,Read&&read,UnlockNative&&unlock){
 if(!enabled)return Off;if(!playable)return Waiting;
 uint32_t scenario=0;uint8_t locked=0;
 if(!read(Scenario,&scenario,4)||scenario<0x10000||scenario>UINT32_MAX-ScenarioLock||!read(scenario+ScenarioLock,&locked,1)||locked>1)return Unreadable;
 // A global-only lock belongs to a modal window/zoom. Do not clear it.
 if(!locked)return Watching;
 if(!unlock(scenario))return WriteFailed;
 uint32_t after=0;uint8_t global=1;
 if(!read(Scenario,&after,4)||after!=scenario||!read(scenario+ScenarioLock,&locked,1)||locked||!read(Address,&global,1)||global)return WriteFailed;
 return Unlocked;
}
}
