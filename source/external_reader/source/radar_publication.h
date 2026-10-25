#pragma once
#include "radar_map.h"
namespace external { namespace radar {
struct PublicationContext {uint32_t house=0,cells=0;int frame=-1;bool active=false;mapbounds::Bounds bounds;bool readable=false;};
template<class Read>PublicationContext publicationContext(Read read){
 PublicationContext out;uint8_t battlefield=0;
 bool fields=read(0xA83D4C,&out.house,sizeof out.house)&&read(Map+0x13C,&out.cells,sizeof out.cells)&&
             read(0xA8ED84,&out.frame,sizeof out.frame)&&read(0xA8E9A0,&battlefield,sizeof battlefield);
 auto display=display::inspect(read);out.bounds=mapbounds::sample(read);
 out.readable=fields&&display.readable&&out.bounds.valid();out.active=out.readable&&battlefield==1&&display.visible();return out;
}
inline bool sameWorld(const Frame&sample,const PublicationContext&current){
 return sample.terrain&&current.active&&current.house==sample.terrain->geo.house&&current.cells==sample.terrain->geo.cells&&current.frame>=sample.frame&&current.bounds.valid()&&current.bounds==sample.terrain->geo.bounds;
}
// Check age when sampling ends, not after disk IO and CPU rasterization. A
// render taking three game ticks is still a valid completed image of this map.
inline bool freshMarkers(const Frame&sample,const PublicationContext&sampled){
 return sameWorld(sample,sampled)&&int64_t(sampled.frame)-sample.frame<=2;
}
}}
