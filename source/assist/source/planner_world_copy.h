#pragma once
#include "planner_types.h"
#include <cstddef>
#include <cstring>

namespace assist { namespace planner {
// The fixed-capacity World is also the IPC layout. Its inactive slots are not
// payload: every consumer and the mailbox publication is bounded by the counts.
// Preserve the retained World's stable addresses without copying all 6 MiB on
// the game thread for a small ore/defense observation. Validate before changing
// any destination field; unused slots deliberately retain their previous bytes.
inline bool copyActiveWorld(World&destination,const World&source){
 if(!validWorld(source))return false;
 if(&destination==&source)return true;
 std::memcpy(destination.actors,source.actors,size_t(source.count)*sizeof(Actor));
 std::memcpy(destination.intents,source.intents,size_t(source.intentCount)*sizeof(Intent));
 std::memcpy(destination.refineries,source.refineries,size_t(source.refineryCount)*sizeof(Refinery));
 std::memcpy(destination.combat,source.combat,size_t(source.combatCount)*sizeof(CombatProbe));
 destination.match=source.match;destination.tick=source.tick;destination.frame=source.frame;
 destination.count=source.count;destination.intentCount=source.intentCount;
 destination.refineryCount=source.refineryCount;destination.inputSerial=source.inputSerial;
 destination.combatCount=source.combatCount;
 return true;
}
} }
