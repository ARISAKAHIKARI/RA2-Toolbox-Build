#pragma once
#include "visual_pose.h"
#include "art_variants.h"
#include <map>
namespace external { namespace nativefoot {
// GPL-3.0 Vera20K foot_depth.rs, read-only adapter for retail4DAFC0,
// 703B10/703CC0/703E70/704000/704240/704350. No locomotor method executes.
using Coord=std::array<int16_t,2>;
inline Coord offset(Coord p,int direction){static constexpr int d[8][2]={{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};return {{int16_t(p[0]+d[direction&7][0]),int16_t(p[1]+d[direction&7][1])}};}
struct Cell {Coord coord{};uint32_t pointer=0,flags=0;int level=0,ramp=0,tile=65535,rock=-1,height=30;bool low=false,dummy=false;};
template<class Read>class Sample {
 GameView<Read>g_;Read read_;Coord own_{},dummy_{};bool bridge_=false;int bridgeSet_=-1;
 std::map<Coord,Cell>&cache_;
 static std::map<Coord,Cell>&cells(Read read,int frame){struct Cache{int frame=-1;uint32_t items=0,scenario=0;std::map<Coord,Cell>values;};static thread_local Cache cache;GameView<Read>g(read);uint32_t items=g.template get<uint32_t>(0x87F924),scenario=g.template get<uint32_t>(0xA8B230);if(cache.frame!=frame||cache.items!=items||cache.scenario!=scenario||cache.values.size()>32768){cache={};cache.frame=frame;cache.items=items;cache.scenario=scenario;}return cache.values;}

 uint32_t indexed(uint32_t vector,int index,int max){if(index<0)return 0;int count=g_.template get<int>(vector+16);uint32_t items=g_.template get<uint32_t>(vector+4);return count>index&&count<=max?g_.template get<uint32_t>(items+uint32_t(index)*4):0;}
 Cell get(Coord c){auto found=cache_.find(c);Cell out;
  if(found!=cache_.end())out=found->second;else{
   int index=int(c[1])*512+c[0];uint32_t items=g_.template get<uint32_t>(0x87F924);out.pointer=index>=0&&index<512*512?g_.template get<uint32_t>(items+uint32_t(index)*4):0;
   if(!out.pointer){out.pointer=0xABDC50;out.dummy=true;}std::array<uint8_t,0x144>bytes{};bool whole=g_.bytes(out.pointer,bytes.data(),bytes.size());auto field=[&](unsigned offset,unsigned size){uint32_t value=0;if(whole)std::memcpy(&value,bytes.data()+offset,size);else g_.bytes(out.pointer+offset,&value,size);return value;};uint32_t xy=field(0x24,4);std::memcpy(out.coord.data(),&xy,4);out.flags=field(0x140,4);
   out.level=int8_t(field(0x11B,1));out.ramp=field(0x11C,1);out.tile=int32_t(field(0x38,4));
   int overlay=int32_t(field(0x44,4));uint32_t overlayType=indexed(0xA83D80,overlay,16384);if(overlay>=0)out.rock=overlayType&&g_.template get<uint8_t>(overlayType+0x2B5)?1:0;
   int tunnel=int16_t(field(0x116,2));out.low=int32_t(field(0xEC,4))==10&&indexed(0x8B4138,tunnel,65536)!=0;
   int tile=out.tile==65535?g_.template get<int>(0xAA10B0,-1):out.tile;unsigned sub=out.tile==65535?0:field(0x11A,1);
   uint32_t type=indexed(0xA8ED28,tile,65536);if(type){type=artvariants::tile(read_,type,sub,c[0],c[1],out.flags);uint32_t tmp=g_.template get<uint32_t>(type+0xA4);int dims[4]{};
    if(g_.bytes(tmp,dims,16)&&dims[0]>0&&dims[1]>0&&dims[0]<=64&&dims[1]<=64){out.height=dims[3];uint32_t frame=g_.template get<uint32_t>(tmp+16+(sub%unsigned(dims[0]*dims[1]))*4);
     if(frame&&(g_.template get<uint32_t>(frame+36)&1))out.height+=g_.template get<int>(frame+4)-g_.template get<int>(frame+24);}}
   cache_.emplace(c,out);
  }if(out.dummy){dummy_=c;out.coord=c;}return out;
 }
 Coord coord(const Cell&c)const{return c.dummy?dummy_:c.coord;}
 bool near(uint32_t mask){auto own=get(own_);if(bridge_)return false;std::array<Cell,4>n{{get(offset(own_,4)),get(offset(own_,0)),get(offset(own_,2)),get(offset(own_,6))}};
  if(own.flags&mask)return true;for(unsigned i=0;i<4;++i)if((n[i].flags&mask)&&bool(n[i].flags&0x800)==(i<2))return true;return false;}
 int column(){get(own_);if(!near(0x100)&&!near(0x400))return 0;auto s=get(offset(own_,4)),e=get(offset(own_,2)),se=get(offset(own_,3));auto column=[&](const Cell&c){int n=int32_t(uint32_t(c.tile)-uint32_t(bridgeSet_)+1u);return c.tile!=255&&c.tile!=65535&&n>=7&&n<=16;};return int(column(s)||column(e))+int(column(se));}
 int tunnel(){if(bridge_)return 0;auto n=get(offset(own_,0)),w=get(offset(own_,6)),c=get(own_);auto selected=c.low?c:n.low?n:w;if(!selected.low||coord(selected)==Coord{{0,0}})return 0;Coord at=coord(selected);auto n1=get(offset(at,0)),w1=get(offset(at,6));auto n2=get(offset(coord(n1),0)),w2=get(offset(coord(w1),6));return n2.low||w2.low;}
 int cliff(){auto c=get(own_);if(bridge_)return 0;auto first=get(offset(own_,3));int score=first.level-c.level>=4?2:0;auto second=get(offset(coord(first),3));return second.level-c.level>=4?1:score;}
 int additional(uint32_t object,uint32_t type,int kind,int base,int frame){auto c=get(own_);
  if(kind==2){uint32_t contacts=g_.template get<uint32_t>(object+0xE4);int count=g_.template get<int>(object+0xE8);uint32_t contact=count>0&&count<=32?g_.template get<uint32_t>(contacts):0;
   if(g_.template get<uint8_t>(object+0x418)&&g_.kind(contact)==3&&g_.template get<int>(contact+0xAC)==16)return base-3;
   if(g_.template get<uint8_t>(type+0xE0E)){uint32_t first=g_.template get<uint32_t>(c.pointer+0xE4);for(int n=0;first&&n<256;++n){if(g_.kind(first)==3){if(g_.template get<uint8_t>(g_.type(first,3)+0x16BB))return base-14;break;}first=g_.template get<uint32_t>(first+0x30);}}}
  if(c.ramp){std::array<Cell,3>neighbors{{get(offset(own_,4)),get(offset(own_,2)),get(offset(own_,3))}};for(const auto&n:neighbors)if(n.rock>=0){if(n.rock)return base+2;break;}}
  unsigned facing=visual::facing(g_,object+0x388,frame),direction=(((facing>>12)+1)>>1)&7;std::array<int,6>probes{{int(direction)+1,int(direction)+7,int(direction),int(direction)+4,int(direction)+3,int(direction)+5}};std::array<Cell,6>neighbors;
  for(unsigned n=0;n<6;++n)neighbors[n]=get(offset(own_,probes[n]));if(c.ramp||std::any_of(neighbors.begin(),neighbors.end(),[](const Cell&n){return n.ramp!=0;}))return base-1;
  int start=direction==0||direction==6?3:direction==2||direction==4?0:-1;if(start<0)return base-1;
  if(c.flags&0x10000)return base-1;for(int n=start;n<start+3;++n)if(neighbors[n].flags&0x10000)return base-1;
  for(int n=start;n<start+3;++n)if(neighbors[n].height>36)return base-2;return base-1;
 }
public:
 struct Result {int adjust=0;bool split=false,shpFlat=false;};
 Sample(Read read,uint32_t object,int frame):g_(read),read_(read),cache_(cells(read,frame)){own_={{int16_t(g_.template get<int>(object+0x9C)/256),int16_t(g_.template get<int>(object+0xA0)/256)}};bridge_=g_.template get<uint8_t>(object+0x8C)!=0;bridgeSet_=g_.template get<int>(0xAA0E28,-1);}
 Result capture(uint32_t object,uint32_t type,int kind,int frame,bool voxel){int z=g_.template get<int>(object+0xA4),base=-screenHeight(z,projectionZScale(read_));Result out{base-(voxel?0:2),false,false};if(kind<0||kind>2)return out;
  auto c=get(own_);bool grounded=voxel;
  if(!voxel&&(kind==1||kind==2)){int height=g_.template get<int>(0x89E7C0,104),floor=0;std::array<double,5>terms{};double scale=g_.template get<double>(0x89E740,1.);if(c.ramp)g_.bytes(0x81C900+(unsigned(c.ramp)-1)*40,terms.data(),sizeof terms);
   grounded=voxelshadow::floorHeight(c.level,height,g_.template get<int>(object+0x9C),g_.template get<int>(object+0xA0),unsigned(c.ramp),scale,terms,floor)&&z==floor+(bridge_?g_.template get<int>(0xAC13BC,416):0);}
  int columns=column(),tunnels=tunnel(),cliffs=cliff();bool byBridge=near(0x100);
  if(grounded){auto multiply=[](int a,int b){return int32_t(uint32_t(a)*uint32_t(b));};int maximum=std::max({multiply(g_.template get<int>(type+0xDC4,5),columns),multiply(g_.template get<int>(type+0xDC8,10),tunnels),multiply(g_.template get<int>(type+0xDC0,10),cliffs),byBridge?g_.template get<int>(type+0xDCC):0});out.adjust=int32_t(uint32_t(additional(object,type,kind,base,frame))+uint32_t(maximum)-(voxel?0u:2u));}
  if(kind==2&&g_.template get<uint8_t>(type+0xE16)){uint32_t contacts=g_.template get<uint32_t>(object+0xE4);int count=g_.template get<int>(object+0xE8);uint32_t contact=count>0&&count<=32?g_.template get<uint32_t>(contacts):0;
   bool factory=g_.template get<uint32_t>(object+0x5A4)&&g_.kind(contact)==3&&g_.template get<uint8_t>(g_.type(contact,3)+0x16BD);out.split=(byBridge&&columns==0)||factory;
   out.shpFlat=!voxel&&!g_.template get<uint8_t>(type+0xCA1)&&byBridge&&columns==0;if(out.shpFlat)out.adjust-=16;}
  return out;
 }
};
}}
