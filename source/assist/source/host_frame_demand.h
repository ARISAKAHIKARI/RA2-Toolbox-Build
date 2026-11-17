#pragma once
#include <cstdint>
#include "compute_policy.h"

namespace assist { namespace compute {
// Only request native frame notices for enabled, host-owned decision lanes.
// The old "computeApplied != All" test woke the game for configurations
// without any enabled consumer. Command/ACK and regular Host wakeups are
// independent of this hint; runtime-only native duties are not moved here.
struct HostFrameDemand {
 uint32_t flags=0,placement=All;
 bool repair=false,unload=false,dodge=false,missile=false;
 bool magnetronGlobal=false,magnetronDrop=false;
 bool pending=false; // An already-created task/receipt or in-flight plan.
};
inline bool needsFrameNotification(const HostFrameDemand&d){
 if(d.pending||plannerFlags(d.flags,d.placement,false))return true;
 return (d.repair&&!inside(d.placement,Repair))||
        (d.unload&&!inside(d.placement,Unload))||
        (d.dodge&&!inside(d.placement,Dodge))||
        (d.missile&&!inside(d.placement,MissileDefense))||
        (d.magnetronGlobal&&!inside(d.placement,MagnetronGlobal))||
        (d.magnetronDrop&&!inside(d.placement,MagnetronDrop));
}
} }
