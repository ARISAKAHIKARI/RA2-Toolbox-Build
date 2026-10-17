#pragma once
#include "planner_types.h"
#include <algorithm>
namespace assist { namespace planner {
// Controller-only indices. No game pointer, allocator or game callback.
struct ActorLookup {
 static constexpr unsigned Capacity=16384;
 uint32_t ids[Capacity]{},epochs[Capacity]{},epoch=1;const Actor* values[Capacity]{};
 static unsigned hash(uint32_t id){return (id*2654435761u)&(Capacity-1);}
 void clear(){if(++epoch==0){std::fill(epochs,epochs+Capacity,0);epoch=1;}}
 void add(const Actor& actor){if(!actor.id)return;unsigned slot=hash(actor.id);
  for(unsigned n=0;n<Capacity;++n,slot=(slot+1)&(Capacity-1))if(epochs[slot]!=epoch||ids[slot]==actor.id){epochs[slot]=epoch;ids[slot]=actor.id;values[slot]=&actor;return;}
 }
 const Actor* find(uint32_t id)const{if(!id)return nullptr;unsigned slot=hash(id);
  for(unsigned n=0;n<Capacity;++n,slot=(slot+1)&(Capacity-1)){if(epochs[slot]!=epoch)return nullptr;if(ids[slot]==id)return values[slot];}return nullptr;
 }
};
struct EnemyGrid {
 static constexpr unsigned Capacity=16384;static constexpr int CellSize=2048;
 uint64_t keys[Capacity]{};uint32_t epochs[Capacity]{},epoch=1;int heads[Capacity]{},next[MaxActors]{},indices[MaxActors]{},count=0;
 static int cell(int64_t coordinate){return int(coordinate>=0?coordinate/CellSize:(coordinate-(CellSize-1))/CellSize);}
 static uint64_t key(int x,int y){return (uint64_t(uint32_t(x))<<32)|uint32_t(y);}
 static unsigned hash(uint64_t k){k^=k>>33;k*=0xff51afd7ed558ccdull;k^=k>>33;k*=0xc4ceb9fe1a85ec53ull;k^=k>>33;return unsigned(k)&(Capacity-1);}
 void clear(){count=0;if(++epoch==0){std::fill(epochs,epochs+Capacity,0);epoch=1;}}
 void add(int index,const Actor& actor){if(index<0||index>=MaxActors||count>=MaxActors)return;uint64_t k=key(cell(actor.x),cell(actor.y));unsigned slot=hash(k);
  for(unsigned n=0;n<Capacity;++n,slot=(slot+1)&(Capacity-1))if(epochs[slot]!=epoch||keys[slot]==k){
   if(epochs[slot]!=epoch){epochs[slot]=epoch;keys[slot]=k;heads[slot]=-1;}
   next[index]=heads[slot];heads[slot]=index;indices[count++]=index;return;
  }
 }
 template<class Visit> void nearby(int x,int y,int range,Visit visit)const{
  if(range<=0||!count)return;
  int left=cell(int64_t(x)-range),right=cell(int64_t(x)+range),top=cell(int64_t(y)-range),bottom=cell(int64_t(y)+range);
  int64_t cells=int64_t(right-left+1)*(bottom-top+1);
  // Long-range mod weapons are cheaper to handle by their whole enemy list.
  if(cells>count*2||cells>4096){for(int i=0;i<count;++i)visit(indices[i]);return;}
  for(int cy=top;cy<=bottom;++cy)for(int cx=left;cx<=right;++cx){uint64_t k=key(cx,cy);unsigned slot=hash(k);
   for(unsigned n=0;n<Capacity;++n,slot=(slot+1)&(Capacity-1)){if(epochs[slot]!=epoch)break;if(keys[slot]==k){for(int i=heads[slot];i>=0;i=next[i])visit(i);break;}}
  }
 }
};
}}
