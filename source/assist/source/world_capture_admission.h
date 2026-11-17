#pragma once
#include "protocol.h"
// Decision-only policy. Visual flags never need the game thread to capture
// a full unit graph when the Host has its own independent read-only VisualCache.
// Do not confuse this with permission to execute commands from Host samples.
namespace assist { namespace capturepolicy {
constexpr uint32_t PassiveVisualFlags=
 Capacity|Iron|Production|AllTransports|Psychic|Crates|TextPanel;
inline bool needsNativeFlagWork(uint32_t flags,bool externalDisplay){
 // A non-external legacy renderer still draws these overlays in the injected
 // Runtime, so keep its original conservative capture admission.
 return (flags & ~(externalDisplay?PassiveVisualFlags:0u))!=0;
}
inline bool passengerGraphNeeded(uint32_t flags,bool externalDisplay){
 // Capacity is purely a visual request with Host display. Passenger event
 // producers and automatic actions are NOT passive; keep their snapshots.
 constexpr uint32_t actions=AutoOreReturn|AdvancedEnter|AircraftSpread|EjectAssist|SmartUnloadOrders;
 return (flags & (actions|(externalDisplay?0u:Capacity)))!=0;
}
static_assert((PassiveVisualFlags&(AutoRepair|EjectAssist|AutoOreReturn|AdvancedInput|UnsafeTech|AntiTheft))==0,
              "Command feature must never be classified as a Host visual-only flag");
}}
