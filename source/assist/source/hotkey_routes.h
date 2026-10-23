#pragma once
#include <stdint.h>
#include <vector>
namespace assist { namespace hotkeyroute {
inline uint16_t canonical(uint16_t key){return key&0x7ff;}
struct Group {int id=0;uint16_t key=0;std::vector<int> actions;std::vector<uint16_t> bindings;uint32_t at=0,match=0;bool waiting=false;};
struct Registry {
 std::vector<Group> groups;
 void clear(){groups.clear();}
 template<class Register> bool add(int id,uint16_t key,bool all,Register install){
  uint16_t physical=canonical(key);key&=0x87ff;if(!(key&255))return true;
  for(auto&g:groups)if(g.key==physical){for(size_t i=0;i<g.actions.size();++i){if(g.actions[i]==id)return true;if(!all&&g.bindings[i]==key)return false;}g.actions.push_back(id);g.bindings.push_back(key);return true;}
  if(!install(id,physical))return false;Group g;g.id=id;g.key=physical;g.actions.push_back(id);g.bindings.push_back(key);groups.push_back(std::move(g));return true;
 }
 const Group* find(int id)const{for(const auto&g:groups)if(g.id==id)return &g;return nullptr;}
 static void filter(const Group&g,bool twice,Group&out){out={};out.key=g.key|(twice?0x8000:0);for(size_t i=0;i<g.actions.size();++i)if(bool(g.bindings[i]&0x8000)==twice)out.actions.push_back(g.actions[i]);}
 bool press(int id,uint32_t now,uint32_t match,Group&out){for(auto&g:groups)if(g.id==id){
  bool hasDouble=false;for(auto k:g.bindings)hasDouble|=(k&0x8000)!=0;
  if(!hasDouble){filter(g,false,out);return !out.actions.empty();}
  if(g.waiting&&g.match==match&&uint32_t(now-g.at)<=350){g.waiting=false;filter(g,true,out);return !out.actions.empty();}
  bool overdue=g.waiting&&g.match==match&&uint32_t(now-g.at)>350;
  g.waiting=true;g.at=now;g.match=match;if(overdue){filter(g,false,out);return !out.actions.empty();}return false;
 }return false;}
 bool expired(uint32_t now,uint32_t match,Group&out){for(auto&g:groups)if(g.waiting){if(g.match!=match){g.waiting=false;continue;}if(uint32_t(now-g.at)>350){g.waiting=false;filter(g,false,out);if(!out.actions.empty())return true;}}return false;}
 void cancel(){for(auto&g:groups)g.waiting=false;}
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
