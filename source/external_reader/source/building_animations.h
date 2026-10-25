#pragma once
#include "game_view.h"
#include <set>
namespace external { namespace animations {
// Native PlayAnim (451890) stores an absolute-coordinate Anim in the building's
// slots without setting OwnerObject. Only SetOwnerObject makes Location relative
// to the owner's CENTER (422BE0 / 447AC0). These are two different relationships.
struct State {uint32_t object=0,id=0,type=0,parent=0,parentId=0,owner=0;int x=0,y=0,z=0,frame=0,zAdjust=0,sort=0,layer=2;bool normalLight=false;};
template<class Read>bool foundation(Read read,uint32_t type,int&w,int&h){
 GameView<Read> g(read);int index=g.template get<int>(type+0xEF0,-1);if(index<0||index>=21)return false;
 w=g.template get<int>(0x8192B8+index*4);h=g.template get<int>(0x819310+index*4);return w>0&&w<=32&&h>0&&h<=32;
}
template<class Read>uint32_t slot(Read read,uint32_t parent,uint32_t object){
 uint32_t slots[29]{};if(!read(parent+0x55C,slots,21*4)||!read(parent+0x5C8,slots+21,8*4))return 0;
 for(int i=0;i<29;++i)if(slots[i]==object)return parent+(i<21?0x55C+i*4:0x5C8+(i-21)*4);return 0;
}
template<class Read>bool capture(Read read,uint32_t object,uint32_t parent,uint32_t parentId,State&out){
 GameView<Read> g(read);if(object<0x10000)return false;
 if(parent&&(parent<0x10000||g.template get<uint32_t>(parent)!=0x7E3EBC||g.template get<uint32_t>(parent+0x10)!=parentId||g.template get<int>(parent+0x6C)<=0))return false;
 uint32_t link=parent?slot(read,parent,object):0,attachedTo=g.template get<uint32_t>(object+0xCC);
 if((parent&&!link)||g.template get<uint32_t>(object)!=0x7E3354||(parent&&attachedTo&&attachedTo!=parent)||!g.template get<uint8_t>(object+0x90)||g.template get<uint8_t>(object+0x179)||g.template get<uint8_t>(object+0x19D))return false;
 State next;next.object=object;next.parent=parent;next.parentId=parentId;next.id=g.template get<uint32_t>(object+0x10);next.type=g.template get<uint32_t>(object+0xC8);next.owner=g.template get<uint32_t>(parent?parent+0x21C:object+0x180);
 if(!next.id||next.type<0x10000)return false;
 int location[3]{};if(!read(object+0x9C,location,sizeof location))return false;
 for(int v:location)if(std::abs(int64_t(v))>512*256)return false;
 if(attachedTo){int base[3]{},w=1,h=1;if((!parent&&(!g.live(attachedTo)||g.template get<int>(attachedTo+0x220)!=0))||!read(attachedTo+0x9C,base,sizeof base))return false;
  if(g.kind(attachedTo)==3&&!foundation(read,g.template get<uint32_t>(attachedTo+0x520),w,h))return false;
  for(int v:base)if(std::abs(int64_t(v))>512*256)return false;
  location[0]+=base[0]+w*128-128;location[1]+=base[1]+h*128-128;location[2]+=base[2];}
 next.x=location[0];next.y=location[1];next.z=location[2];
 if(next.x<0||next.y<0||next.x>=512*256||next.y>=512*256)return false;
 // Draw_It 42303F always uses Value + Type.Start, including paused/power-off
 // animations. PausedAnimFrame is saved update state, not a second draw frame.
 int value=g.template get<int>(object+0xAC,-1),start=g.template get<int>(next.type+0x2B4,-1);
 if(value<0||value>=8192||start<0||start>=8192)return false;next.frame=value+start;
 next.zAdjust=g.template get<int>(object+0x100);next.normalLight=g.template get<uint8_t>(next.type+0x35D)!=0;
 next.sort=g.template get<int>(object+0x104);next.layer=attachedTo?2:g.template get<int>(next.type+0x364,2);
 if(next.frame>=8192||std::abs(int64_t(next.zAdjust))>8192||std::abs(int64_t(next.sort))>32768||next.layer<0||next.layer>4)return false;
 if((parent&&(g.template get<uint32_t>(parent+0x10)!=parentId||g.template get<uint32_t>(link)!=object))||g.template get<uint32_t>(object+0x10)!=next.id||g.template get<uint32_t>(object+0xC8)!=next.type||g.template get<uint32_t>(object+0xCC)!=attachedTo)return false;
 out=next;return true;
}
template<class Read>std::vector<State> attached(Read read,uint32_t parent,uint32_t parentId){
 std::vector<State> out;uint32_t slots[29]{};
 if(parent<0x10000||!read(parent+0x55C,slots,21*4)||!read(parent+0x5C8,slots+21,8*4))return out;
 std::set<uint32_t> seen;for(auto pointer:slots){State state;if(pointer&&seen.insert(pointer).second&&capture(read,pointer,parent,parentId,state))out.push_back(state);}return out;
}
// Transient explosions/warhead effects have no Building::Anims slot.
template<class Read>std::vector<State> all(Read read){
 GameView<Read> g(read);std::vector<State> out;for(auto p:g.vec(0xA8E9A8,16384)){State s;if(capture(read,p,0,0,s))out.push_back(s);}return out;
}
}}
