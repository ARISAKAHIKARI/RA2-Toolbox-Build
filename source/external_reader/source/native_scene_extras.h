#pragma once
#include "game_view.h"
#include "native_feedback.h"
#include <algorithm>
#include <array>
#include <set>
namespace external { namespace sceneextras {
// Read-only adapters for the native owners. Never retain an engine pointer as
// the lifetime of a host image, and never advance timers from a host frame.
struct Record {uint32_t type=0;int frame=0,noZShape=0,zAdjust=0;};
struct Frozen {uint32_t object=0,id=0,owner=0;nativefeedback::Point3 point{};int level=0;std::vector<Record> records;};
struct TrailNode {nativefeedback::Point3 point{};int strength=0;};
struct Trail {uint32_t object=0,owner=0,color=0;int slot=0;std::array<TrailNode,32>nodes{};};
struct Beacon {uint32_t object=0,owner=0;nativefeedback::Point3 point{};bool selected=false;};
struct RadarEvent {int type=0,x=0,y=0;float radius=0,rotation=0,fade=0;};
struct Sparkle {int x=0,y=0,level=0,dx=0,dy=0,phase=0;uint32_t base=0,peak=0;};
struct Chrono {int phase=0,remaining=0,duration=0;uint32_t source=0;bool operator==(const Chrono&o)const{return phase==o.phase&&remaining==o.remaining&&duration==o.duration&&source==o.source;}};
struct Snapshot {std::vector<Sparkle>sparkles;std::vector<Frozen>frozen;std::vector<Trail>trails;std::vector<Beacon>beacons;std::vector<RadarEvent>radar;Chrono chrono;uint32_t beaconShape=0,radarShape=0;int beaconFrames=0,radarFrames=0;bool frozenValid=false,trailsValid=false,beaconsValid=false,radarValid=false,chronoValid=false;};
template<class T>T field(const uint8_t*p){T out{};std::memcpy(&out,p,sizeof out);return out;}
inline bool point(nativefeedback::Point3 p){return p.x>=0&&p.y>=0&&p.x<512*256&&p.y<512*256&&p.z>=-16384&&p.z<=65536;}
template<class Read,class Visit>bool scan(Read read,uint32_t array,int max,Visit visit){GameView g(read);std::vector<uint32_t>values,after;if(!g.vecChecked(array,values,max))return false;std::set<uint32_t>seen;for(auto p:values)if(p<0x10000||!seen.insert(p).second||!visit(p))return false;return g.vecChecked(array,after,max)&&values==after;}
template<class Read>Snapshot capture(Read read,uint32_t observer,int frame,bool fog,bool particles){
 Snapshot out;GameView g(read);uint32_t gameWindow=0;
 // DrawAll4D1890 tests the full HWND, not a FogOfWar byte. A valid window
 // with low byte zero still draws saved buildings. Unknown is not absence.
 const bool windowKnown=!fog||read(0xB73550,&gameWindow,sizeof gameWindow);out.frozenValid=windowKnown&&(!fog||!gameWindow);
 if(fog&&windowKnown&&gameWindow){
  out.frozenValid=scan(read,0x8B3D10,16384,[&](uint32_t p){std::array<uint8_t,0x78>d{},after{};if(!read(p,d.data(),d.size())||field<uint32_t>(d.data())!=0x7E8B38)return false;
   if(!d[0x74]||field<int>(d.data()+0x30)!=6)return true;Frozen v;v.object=p;v.id=field<uint32_t>(d.data()+0x10);v.owner=field<uint32_t>(d.data()+0x28);v.point=field<nativefeedback::Point3>(d.data()+0x34);v.level=field<int8_t>(d.data()+0x50);
   int n=field<int>(d.data()+0x6C),cap=field<int>(d.data()+0x64);uint32_t items=field<uint32_t>(d.data()+0x60);if(!point(v.point)||n<1||n>64||cap<n||cap>4096||items<0x10000)return false;
   v.records.resize(n);static_assert(sizeof(Record)==16);if(!read(items,v.records.data(),v.records.size()*16))return false;
   for(const auto&r:v.records)if(r.type<0x10000||r.frame<0||r.frame>=8192||std::abs(int64_t(r.zAdjust))>8192)return false;
   if(!read(p,after.data(),after.size())||d!=after)return false;out.frozen.push_back(std::move(v));return true;});
  if(!out.frozenValid)out.frozen.clear();
  auto key=[](const Frozen&v){return int32_t(0x7fffffffu-uint32_t(int16_t(v.point.x/256))*0x944au-uint32_t(int16_t(v.point.y/256))*0x93b6u-6u);};
  std::stable_sort(out.frozen.begin(),out.frozen.end(),[&](const auto&a,const auto&b){return key(a)<key(b);});
 }
 if(particles){out.trailsValid=scan(read,0xABCB78,4096,[&](uint32_t p){std::array<uint8_t,0x210>d{},after{};if(!read(p,d.data(),d.size()))return false;Trail t;t.object=p;t.owner=field<uint32_t>(d.data()+4);t.color=(uint32_t(d[0])<<16)|(uint32_t(d[1])<<8)|d[2];t.slot=field<int>(d.data()+12);if(t.slot<0||t.slot>31)return false;
   std::memcpy(t.nodes.data(),d.data()+16,512);for(const auto&n:t.nodes)if(n.strength<0||n.strength>256||((n.point.x||n.point.y||n.point.z)&&!point(n.point)))return false;
   if(!read(p,after.data(),after.size())||d!=after)return false;out.trails.push_back(t);return true;});if(!out.trailsValid)out.trails.clear();}
 std::array<uint32_t,24>beacons{},afterBeacons{};auto houses=g.vec(0xA80228,128);out.beaconsValid=read(0x89C3B0,beacons.data(),96);
 if(out.beaconsValid){out.beaconShape=g.template get<uint32_t>(0x89C474);out.radarShape=g.template get<uint32_t>(0x89C478);out.beaconFrames=g.template get<int>(0x89C41C);out.radarFrames=g.template get<int>(0x89C428);
  for(auto p:beacons){if(!p)continue;std::array<uint8_t,0x114>d{};if(p<0x10000||!read(p,d.data(),d.size())){out.beaconsValid=false;break;}if(!(d[12]&1))continue;int owner=field<int>(d.data()+0x110);if(owner<0||size_t(owner)>=houses.size()){out.beaconsValid=false;break;}uint32_t house=houses[owner];if(g.template get<uint8_t>(house+0x1F5)||!g.allied(house,observer)||!g.allied(observer,house))continue;
   Beacon b{p,house,field<nativefeedback::Point3>(d.data()),(d[12]&2)!=0};if(!point(b.point)){out.beaconsValid=false;break;}out.beacons.push_back(b);}
  out.beaconsValid&=read(0x89C3B0,afterBeacons.data(),96)&&beacons==afterBeacons;}
 if(!out.beaconsValid){out.beacons.clear();out.beaconFrames=out.radarFrames=0;}
 out.radarValid=scan(read,0xB04DA8,4096,[&](uint32_t p){std::array<uint8_t,0x40>d{},after{};if(!read(p,d.data(),d.size()))return false;
  if(!d[0x3C]&&!assist::remaining(field<int>(d.data()+0x30),field<int>(d.data()+0x38),frame))return true;
  RadarEvent e{field<int>(d.data()),field<int>(d.data()+4),field<int>(d.data()+8),field<float>(d.data()+12),field<float>(d.data()+16),field<float>(d.data()+24)};
  if(e.type<0||e.type>=17||std::abs(int64_t(e.x))>4096||std::abs(int64_t(e.y))>4096||!std::isfinite(e.radius)||e.radius<0||e.radius>8192||!std::isfinite(e.rotation)||std::abs(e.rotation)>1e6||!std::isfinite(e.fade))return false;
  if(!read(p,after.data(),after.size())||d!=after)return false;out.radar.push_back(e);return true;});if(!out.radarValid)out.radar.clear();
 Chrono verify;auto chrono=[&](Chrono&v){return read(0xA9FAB0,&v.phase,4)&&read(0xA9FA98,&v.remaining,4)&&read(0xA9FAA8,&v.duration,4)&&read(0xA9FA9C,&v.source,4);};
 out.chronoValid=chrono(out.chrono)&&chrono(verify)&&out.chrono==verify&&out.chrono.phase>=0&&out.chrono.phase<=3&&out.chrono.remaining>=0&&out.chrono.duration>=0&&out.chrono.duration<=1000000;if(!out.chronoValid)out.chrono={};
 return out;
}
}}
