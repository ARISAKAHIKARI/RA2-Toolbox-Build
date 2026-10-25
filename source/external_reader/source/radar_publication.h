#pragma once
#include "radar_map.h"
namespace external { namespace radar {
struct PublicationContext {uint32_t house=0,cells=0;int frame=-1;bool active=false;};
template<class Read>PublicationContext publicationContext(Read read){
 GameView<Read> game(read);return {game.template get<uint32_t>(0xA83D4C),game.template get<uint32_t>(Map+0x13C),game.template get<int>(0xA8ED84,-1),game.template get<uint8_t>(0xA8E9A0)==1&&display::inspect(read).visible()};
}
inline bool sameWorld(const Frame&sample,const PublicationContext&current){
 return sample.terrain&&current.active&&current.house==sample.terrain->geo.house&&current.cells==sample.terrain->geo.cells&&current.frame>=sample.frame;
}
// Check age when sampling ends, not after disk IO and CPU rasterization. A
// render taking three game ticks is still a valid completed image of this map.
inline bool freshMarkers(const Frame&sample,const PublicationContext&sampled){
 return sameWorld(sample,sampled)&&int64_t(sampled.frame)-sample.frame<=2;
}
}}
