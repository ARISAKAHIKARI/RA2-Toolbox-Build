#pragma once
#include "radar_map.h"
namespace external {namespace radar {
// Prepare common building animation facts once at observation time. Consumers
// keep the original source stamp; they never resample these per viewport.
template<class Read>void preparePoses(Frame&raw,Read read){
 if(raw.posesPrepared)return;GameView game(read);const uint32_t rules=game.template get<uint32_t>(0x8871E0);
 double yellow=rules>=0x10000?game.template get<double>(rules+0x1700,.5):.5;
 double red=rules>=0x10000?game.template get<double>(rules+0x1708,.25):.25;
 if(!std::isfinite(yellow)||yellow<0||yellow>1)yellow=.5;
 if(!std::isfinite(red)||red<0||red>1)red=.25;
 for(auto&m:raw.markers)if(m.kind==3){m.pose.frame=buildings::frame(read,m.object,m.type,yellow,red);const auto state=buildings::state(read,m.object,m.type,yellow);
  m.pose.buildingFlags=state.flags;m.pose.doorFrame=state.doorFrame;m.pose.variant=buildings::buildup(game,m.object)?-2:-1;
 }
 raw.posesPrepared=true;
}
}}
