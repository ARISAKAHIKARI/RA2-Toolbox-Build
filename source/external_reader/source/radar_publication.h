#pragma once
#include "radar_map.h"
namespace external { namespace radar {
struct PublicationContext {uint32_t house=0,cells=0;int frame=-1;bool active=false;mapbounds::Bounds bounds;bool readable=false;int rawWidth=0,rawHeight=0;};
template<class Read>PublicationContext publicationContext(Read read){
 PublicationContext out;uint8_t battlefield=0;
 bool fields=read(0xA83D4C,&out.house,sizeof out.house)&&read(Map+0x13C,&out.cells,sizeof out.cells)&&
             read(0xA8ED84,&out.frame,sizeof out.frame)&&read(0xA8E9A0,&battlefield,sizeof battlefield)&&
             read(Map+0x1240,&out.rawWidth,sizeof out.rawWidth)&&read(Map+0x1244,&out.rawHeight,sizeof out.rawHeight);
 auto display=display::inspect(read);out.bounds=mapbounds::sample(read);
 out.readable=fields&&display.readable&&out.bounds.valid()&&out.rawWidth>0&&out.rawHeight>0;
 out.active=out.readable&&battlefield==1&&display.visible();return out;
}
inline bool sameWorld(const Frame&sample,const PublicationContext&current){
 return sample.terrain&&current.active&&current.house==sample.terrain->geo.house&&current.cells==sample.terrain->geo.cells&&current.frame>=sample.frame&&current.bounds.valid()&&current.bounds==sample.terrain->geo.bounds&&current.rawWidth==sample.terrain->geo.rawWidth&&current.rawHeight==sample.terrain->geo.rawHeight;
}
// Check age when sampling ends, not after disk IO and CPU rasterization. A
// render taking three game ticks is still a valid completed image of this map.
inline bool freshMarkers(const Frame&sample,const PublicationContext&sampled){
 return sameWorld(sample,sampled)&&int64_t(sampled.frame)-sample.frame<=2;
}
// A completed, validated bank crosses a thread boundary. Its usable age is
// wall time: two game ticks can be only a few milliseconds at fast game speed.
inline bool recentMarkers(const Frame&sample,const PublicationContext&current,uint32_t now,uint32_t maximumAge=100){
 return sample.markerScanValid&&sameWorld(sample,current)&&uint32_t(now-sample.tick)<=maximumAge;
}
}}
