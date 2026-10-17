#pragma once
#include <stdint.h>
#include <vector>
namespace assist { namespace hotkeyroute {
inline uint16_t canonical(uint16_t key){return key&0x7ff;}
struct Group {int id=0;uint16_t key=0;std::vector<int> actions;};
struct Registry {
 std::vector<Group> groups;
 void clear(){groups.clear();}
 template<class Register> bool add(int id,uint16_t key,bool all,Register install){
  key=canonical(key);if(!(key&255))return true;
  for(auto&g:groups)if(g.key==key){if(!all)return false;for(int action:g.actions)if(action==id)return true;g.actions.push_back(id);return true;}
  if(!install(id,key))return false;groups.push_back({id,key,{id}});return true;
 }
 const Group* find(int id)const{for(const auto&g:groups)if(g.id==id)return &g;return nullptr;}
};
struct Action {int id=0;uint32_t tick=0,match=0;};
struct Pending {
 static constexpr unsigned Capacity=128;Action items[Capacity]{};unsigned head=0,count=0;
 void clear(){head=count=0;}
 bool add(const Group&g,uint32_t tick,uint32_t match){
  if(!match||g.actions.size()>Capacity-count)return false;
  for(int id:g.actions)items[(head+count++)%Capacity]={id,tick,match};return true;
 }
 bool pop(uint32_t now,uint32_t match,Action&out){
  while(count){out=items[head];head=(head+1)%Capacity;--count;if(out.match==match&&uint32_t(now-out.tick)<=2500)return true;}return false;
 }
};
}}
