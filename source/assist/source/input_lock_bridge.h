#pragma once
#include "input_lock.h"
static bool unlockScenarioInput(uint32_t scenario){
 static constexpr uint8_t head[]={0xA1,0x30,0xB2,0xA8,0,0x80,0xB8,0xA2,0x35,0,0,1,0x75,0x29};
 static constexpr uint8_t tail[]={0xA1,0x30,0xB2,0xA8,0,0xC6,0x80,0xA2,0x35,0,0,0,0x8B,0x0D,0x70,0xF7,0x87,0,0xC6,5,0x9C,0xED,0xA8,0,0,0xE8,0x3B,0xB4,0xEC,0xFF};
 if(!simulationReady()||get<uint32_t>(inputlock::Scenario)!=scenario||
  !debugentries::pattern(debugentries::Kind::Call,inputlock::Unlock,head,sizeof head,read)||
  !debugentries::pattern(debugentries::Kind::Call,0x6842C7,tail,sizeof tail,read))return false;
 const uint32_t mouse=get<uint32_t>(0x887640),keyboard=get<uint32_t>(0x87F770);
 if(mouse<0x10000||keyboard<0x10000)return false;
 const uint32_t table=get<uint32_t>(mouse),depth=get<uint32_t>(table+0x28),pop=get<uint32_t>(table+0x10);
 if(!nativeExecutableRange(depth,1)||!nativeExecutableRange(pop,1))return false;
 const int locks=call<int>(depth,mouse);if(locks<0||locks>64)return false;
 if(!simulationReady()||get<uint32_t>(inputlock::Scenario)!=scenario)return false;
 reinterpret_cast<void(__cdecl*)()>(uintptr_t(inputlock::Unlock))();
 return call<int>(depth,mouse)==0;
}
static void inputLockTick(){
 if(!sh)return;
 const int state=inputlock::maintain(sh->keepInputUnlocked,
  sh->foreground&&!snap.chat&&simulationReady(),readLiveWorld,unlockScenarioInput);
 sh->inputUnlockStatus=state;if(state==inputlock::Unlocked)++sh->inputUnlockCount;
}
