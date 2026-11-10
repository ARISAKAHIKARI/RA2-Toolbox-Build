#pragma once
#include "input_lock.h"
static void inputLockTick(){
 if(!sh)return;
 const int state=inputlock::maintain(sh->keepInputUnlocked,
  sh->foreground&&!snap.chat&&simulationReady(),readLiveWorld,controlHouseFlagWrite);
 sh->inputUnlockStatus=state;if(state==inputlock::Unlocked)++sh->inputUnlockCount;
}
