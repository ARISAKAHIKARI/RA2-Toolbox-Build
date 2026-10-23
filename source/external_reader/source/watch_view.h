#pragma once
#include "passenger_bubbles.h"
#include "radar_map.h"
namespace external { namespace watch {
struct Target {uint32_t pointer=0,id=0;bool operator==(const Target&o)const{return pointer==o.pointer&&id==o.id;}explicit operator bool()const{return pointer&&id;}};
struct Actor {Target target;uint32_t type=0,owner=0,color=0;int x=0,y=0,z=0,kind=-1,health=0,stage=0;uint16_t name[40]{};};
struct Tile {uint32_t image=0,overlay=0,type=0,overlayType=0;int sub=0,level=0,overlayFrame=0;};
struct Frame {Target focus;int cx=0,cy=0;int radius=5,side=11;std::vector<uint32_t> cells=std::vector<uint32_t>(121);std::vector<Tile> tiles=std::vector<Tile>(121);std::vector<Actor> actors;std::vector<passengers::Group> passengers;bool dangerous=false;};
template<class Read> bool valid(GameView<Read>&g,Target t){return t&&g.live(t.pointer)&&g.template get<uint32_t>(t.pointer+0x10)==t.id&&!g.template get<uint32_t>(t.pointer+0x11C)&&g.template get<int>(t.pointer+0x220)==0;}
template<class Read> Target manual(Read read){
 GameView<Read> g(read);for(auto p:g.vec(0xA8EC78))if(g.template get<uint8_t>(p+0x431)){Target t{p,g.template get<uint32_t>(p+0x10)};if(valid(g,t))return t;}
 for(auto p:g.vec(0xA8ECB8)){Target t{p,g.template get<uint32_t>(p+0x10)};if(valid(g,t))return t;}return {};
}
template<class Read> bool danger(Read read,GameView<Read>&g,Target t,uint32_t house){
 if(!valid(g,t)||!g.enemy(t.pointer,house)||(g.kind(t.pointer)!=0&&g.kind(t.pointer)!=2))return false;
 std::vector<passengers::Group> groups;bool dangerous=false;return passengers::contents(read,t.pointer,groups,dangerous)&&dangerous;
}
template<class Read> Target choose(Read read,Target pinned,Target prior,bool automatic){
 GameView<Read> g(read);if(pinned)return valid(g,pinned)?pinned:Target{};if(!automatic)return {};
 uint32_t house=g.template get<uint32_t>(0xA83D4C);if(danger(read,g,prior,house))return prior;
 for(auto p:g.vec(0xA8EC78)){Target t{p,g.template get<uint32_t>(p+0x10)};if(danger(read,g,t,house))return t;}return {};
}
template<class Read> Frame sample(Read read,Target focus,bool reveal,int radius=5){
 Frame out;out.radius=std::clamp(radius,5,24);out.side=2*out.radius+1;out.cells.resize(out.side*out.side);out.tiles.resize(out.side*out.side);GameView<Read> g(read);if(!valid(g,focus))return out;
 out.cx=g.template get<int>(focus.pointer+0x9C,-256)/256;out.cy=g.template get<int>(focus.pointer+0xA0,-256)/256;if(out.cx<0||out.cy<0||out.cx>=512||out.cy>=512)return {};
 uint32_t cells=g.template get<uint32_t>(radar::Map+0x13C);int count=g.template get<int>(radar::Map+0x140);if(cells<0x10000||count<1||count>512*512)return {};
 auto tileTypes=g.vec(0xA8ED28),overlayTypes=g.vec(0xA83D80);
 out.focus=focus;passengers::contents(read,focus.pointer,out.passengers,out.dangerous);
 for(int y=-out.radius;y<=out.radius;++y)for(int x=-out.radius;x<=out.radius;++x){int cx=out.cx+x,cy=out.cy+y;if(cx<0||cy<0||cx>=512||cy>=512||cy*512+cx>=count)continue;
  uint32_t p=g.template get<uint32_t>(cells+uint32_t(cy*512+cx)*4);if(p<0x10000||g.template get<int16_t>(p+0x24)!=cx||g.template get<int16_t>(p+0x26)!=cy)continue;
  bool seen=g.template get<int8_t>(p+0x120)==-1,fog=g.template get<int8_t>(p+0x121)!=-1;if(!reveal&&!seen)continue;
  auto&t=out.tiles[(y+out.radius)*out.side+x+out.radius];t.level=g.template get<int8_t>(p+0x11B);t.sub=g.template get<uint8_t>(p+0x11A);
  int type=g.template get<int>(p+0x38,-1);if(type==65535){type=g.template get<int>(0xAA10B0,-1);t.sub=0;}
  if(type>=0&&size_t(type)<tileTypes.size()){t.type=tileTypes[type];t.image=g.template get<uint32_t>(t.type+0xA4); }
  int overlay=g.template get<int>(p+0x44,-1);if(overlay>=0&&size_t(overlay)<overlayTypes.size()){t.overlayType=overlayTypes[overlay];t.overlay=g.template get<uint32_t>(t.overlayType+0xA4);t.overlayFrame=g.template get<uint8_t>(p+0x11E);}
  out.cells[(y+out.radius)*out.side+x+out.radius]=0xff000000u|radar::terrainColor(g.template get<int>(p+0xEC,-1),g.template get<uint8_t>(p+0x11B),!reveal&&fog);
 }
 for(auto p:g.vec(0xA8EC78)){Target t{p,g.template get<uint32_t>(p+0x10)};if(!valid(g,t))continue;
  int x=g.template get<int>(p+0x9C,-1),y=g.template get<int>(p+0xA0,-1);if(x<0||y<0)continue;int cx=x/256-out.cx,cy=y/256-out.cy;if(cx<-out.radius||cx>out.radius||cy<-out.radius||cy>out.radius||!out.cells[(cy+out.radius)*out.side+cx+out.radius])continue;
  Actor a;a.target=t;a.x=x;a.y=y;a.kind=g.kind(p);a.type=g.type(p,a.kind);a.owner=g.template get<uint32_t>(p+0x21C);a.health=g.template get<int>(p+0x6C);a.z=g.template get<int>(p+0xA4);uint8_t rgb[3]{180,180,180};read(a.owner+22265,rgb,3);a.color=0xff000000u|(uint32_t(rgb[0])<<16)|(uint32_t(rgb[1])<<8)|rgb[2];
  uint32_t name=g.template get<uint32_t>(a.type+0x60);if(!name||!read(name,a.name,sizeof a.name)){char id[24]{};read(a.type+0x24,id,23);for(int i=0;i<23;++i)a.name[i]=uint8_t(id[i]);}a.name[39]=0;
  if(out.actors.size()<256)out.actors.push_back(a);else if(t==focus)out.actors.back()=a;
 }
 // Trees and static terrain are ObjectClass instances, outside TechnoClass::Array.
 for(auto p:g.vec(0xA8E988)){if(g.template get<uint32_t>(p)!=0x7F522C||g.template get<uint8_t>(p+0xCD)||g.template get<int>(p+0x6C)<=0)continue;
  int x=g.template get<int>(p+0x9C,-1),y=g.template get<int>(p+0xA0,-1);if(x<0||y<0)continue;int cx=x/256-out.cx,cy=y/256-out.cy;if(cx<-out.radius||cx>out.radius||cy<-out.radius||cy>out.radius||!out.cells[(cy+out.radius)*out.side+cx+out.radius])continue;
  Actor a;a.target={p,g.template get<uint32_t>(p+0x10)};a.x=x;a.y=y;a.z=g.template get<int>(p+0xA4);a.type=g.template get<uint32_t>(p+0xC8);a.kind=4;a.stage=g.template get<int>(p+0xAC);a.color=0xff83a95bu;out.actors.push_back(a);if(out.actors.size()>=512)break;
 }
 if(!valid(g,focus))return {};std::sort(out.actors.begin(),out.actors.end(),[](const Actor&a,const Actor&b){return int64_t(a.x)+a.y<int64_t(b.x)+b.y;});return out;
}
}}
