#pragma once
#include "game_view.h"
#include <set>
namespace external { namespace animations {
// YRpp d9795546 layout, and Phobos e81bf980 AnimClass::_GetCenterCoords:
// an attached Anim.Location is relative to OwnerObject::GetCoords().
struct State {uint32_t object=0,id=0,type=0,parent=0,parentId=0,owner=0;int x=0,y=0,z=0,frame=0,zAdjust=0;bool normalLight=false;};
template<class Read>bool capture(Read read,uint32_t object,uint32_t parent,uint32_t parentId,State&out){
 GameView<Read> g(read);if(parent<0x10000||object<0x10000||g.template get<uint32_t>(parent)!=0x7E3EBC||g.template get<uint32_t>(parent+0x10)!=parentId||g.template get<int>(parent+0x6C)<=0)return false;
 if(g.template get<uint32_t>(object+0xCC)!=parent||!g.template get<uint8_t>(object+0x90)||g.template get<uint8_t>(object+0x179)||g.template get<uint8_t>(object+0x19D))return false;
 State next;next.object=object;next.parent=parent;next.parentId=parentId;next.id=g.template get<uint32_t>(object+0x10);next.type=g.template get<uint32_t>(object+0xC8);next.owner=g.template get<uint32_t>(parent+0x21C);
 if(!next.id||next.type<0x10000)return false;
 int local[3]{},base[3]{};if(!read(object+0x9C,local,sizeof local)||!read(parent+0x9C,base,sizeof base))return false;
 for(int i=0;i<3;++i)if(std::abs(int64_t(local[i]))>32768||std::abs(int64_t(base[i]))>512*256)return false;
 next.x=base[0]+local[0];next.y=base[1]+local[1];next.z=base[2]+local[2];
 if(next.x<0||next.y<0||next.x>=512*256||next.y>=512*256)return false;
 next.frame=g.template get<uint8_t>(object+0x11A)?g.template get<int>(object+0x11C,-1):g.template get<int>(object+0xAC,-1);
 next.zAdjust=g.template get<int>(object+0x100);next.normalLight=g.template get<uint8_t>(next.type+0x35D)!=0;
 if(next.frame<0||next.frame>=8192||std::abs(int64_t(next.zAdjust))>8192)return false;
 if(g.template get<uint32_t>(parent+0x10)!=parentId||g.template get<uint32_t>(object+0x10)!=next.id||g.template get<uint32_t>(object+0xC8)!=next.type||g.template get<uint32_t>(object+0xCC)!=parent)return false;
 out=next;return true;
}
template<class Read>std::vector<State> attached(Read read,uint32_t parent,uint32_t parentId){
 std::vector<State> out;uint32_t slots[29]{};
 if(parent<0x10000||!read(parent+0x55C,slots,21*4)||!read(parent+0x5C8,slots+21,8*4))return out;
 std::set<uint32_t> seen;for(auto pointer:slots){State state;if(pointer&&seen.insert(pointer).second&&capture(read,pointer,parent,parentId,state))out.push_back(state);}return out;
}
}}
