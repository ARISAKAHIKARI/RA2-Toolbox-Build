#pragma once
#include "game_view.h"
#include <vector>
namespace external { namespace artvariants {
// The map's stored overlay identifies a resource pool; it is not necessarily
// the SHP the native DrawOverlay chooses at this cell. See the native replay.
struct Pool {int first=0,flat=0,slopes=0;};
template<class Read>std::vector<Pool> pools(Read read,const std::vector<uint32_t>&types){
 GameView<Read> g(read);std::vector<Pool> out;
 for(auto p:g.vec(0xB0F4E8,256)){
  uint32_t base=0;int first=-1,flat=0,slopes=0;
  if(!read(p+0xE0,&base,4)||base<0x10000||!read(base+0x294,&first,4)||
     !read(p+0xE8,&flat,4)||!read(p+0xEC,&slopes,4)||first<0||flat<1||flat>256||slopes<0||slopes>256||
     uint64_t(first)+unsigned(flat)+unsigned(slopes)>types.size()||types[first]!=base)continue;
  out.push_back({first,flat,slopes});
 }return out;
}
template<class Read>int overlay(Read read,const std::vector<uint32_t>&types,const std::vector<Pool>&resources,int stored,int x,int y,unsigned slope){
 if(stored<0||size_t(stored)>=types.size())return stored;
 uint8_t resource=0;if(!read(types[stored]+0x2A9,&resource,1)||!resource)return stored;
 if(x<0||y<0||x>=512||y>=512)return stored;
 int product=x*y; // Legal CellStruct coordinates are nonnegative signed shorts.
 for(const auto&pool:resources)if(stored>=pool.first&&stored<pool.first+pool.flat+pool.slopes){
  int selected=pool.first;
  if(slope){int perSlope=pool.slopes/4;if(perSlope<1)return stored;
   selected+=pool.flat+product%perSlope+(int(slope)-1)*perSlope;
  }else selected+=product%pool.flat;
  return selected>=pool.first&&selected<pool.first+pool.flat+pool.slopes?selected:stored;
 }return stored;
}
// Native Cell::DrawIsoTile selects a linked TMP variation before drawing.
// Preserve its native subtile/root-coordinate adjustment. Destructible
// variants instead follow the live damaged flag.
template<class Read>uint32_t tile(Read read,uint32_t type,unsigned sub,int x,int y,uint32_t flags){
 int count=0;if(type<0x10000||!read(type+0x2F0,&count,4)||count<=1||count>64)return type;
 uint32_t shape=0,h[2]{},frame=0,frameFlags=0;
 if(!read(type+0xA4,&shape,4)||shape<0x10000||!read(shape,h,sizeof h)||!h[0]||!h[1]||h[0]>64||h[1]>64)return type;
 unsigned index=sub%(h[0]*h[1]);if(!read(shape+16+index*4,&frame,4)||frame<0x10000||!read(frame+36,&frameFlags,4))return type;
 int variation=0;
 if(frameFlags&4)variation=int((flags>>13)&1);
 else{
  if(sub){x=int(int16_t(x-int(sub%h[0])))/int(h[0]);y=int(int16_t(y-int(sub/h[0])))/int(h[1]);}
  uint32_t table=0x81CCA8;unsigned at=(unsigned(x)&3)|((unsigned(y)&3)<<2);
  if(count>4){uint8_t ready=0;if(!read(0x89E7C7,&ready,1)||!ready)return type;table=0x89E620;at=(unsigned(x)&7)|((unsigned(y)&7)<<3);}
  if(!read(table+at*4,&variation,4)||variation<0)return type;
 }
 variation%=count;uint32_t selected=type;
 for(int n=0;n<variation;++n){uint32_t next=0;if(!read(selected+0x2BC,&next,4)||next<0x10000||next==selected)return type;selected=next;}
 return selected;
}
}}
