#pragma once
#include <stdint.h>
#include <vector>
#include "../../shared/source/key_gesture.h"
namespace assist { namespace hotkeyroute {
inline uint16_t canonical(uint16_t key){return key&0x7ff;}
struct Group {int id=0;uint16_t key=0;std::vector<int> actions;std::vector<uint16_t> bindings;uint32_t at=0,match=0;bool waiting=false,down=false,second=false,singleReleased=false,canDouble=false,suppressed=false,firstReleaseValid=false;};
struct Registry {
 std::vector<Group> groups;bool synchronized=false;
 void clear(){groups.clear();synchronized=false;}
 template<class Register> bool add(int id,uint16_t key,bool all,Register install){
  uint16_t physical=canonical(key);key=keygesture::normalize(key);if(!(key&255))return true;
  if(keygesture::effective(key)==keygesture::Mode::Hold)return false;
  for(auto&g:groups)if(g.key==physical){for(size_t i=0;i<g.actions.size();++i){if(g.actions[i]==id)return true;if(!all&&g.bindings[i]==key)return false;}g.actions.push_back(id);g.bindings.push_back(key);return true;}
  if(!install(id,physical))return false;Group g;g.id=id;g.key=physical;g.actions.push_back(id);g.bindings.push_back(key);groups.push_back(std::move(g));return true;
 }
 const Group* find(int id)const{for(const auto&g:groups)if(g.id==id)return &g;return nullptr;}
 static void append(const Group&g,bool twice,keygesture::Mode phase,Group&out){out.key=g.key|(twice?0x8000:0);if(twice&&phase==keygesture::Mode::Release&&!g.firstReleaseValid)return;for(size_t i=0;i<g.actions.size();++i)if(bool(g.bindings[i]&0x8000)==twice&&keygesture::effective(g.bindings[i])==phase){out.actions.push_back(g.actions[i]);out.bindings.push_back(g.bindings[i]);}}
 static void filter(const Group&g,bool twice,Group&out){out={};append(g,twice,keygesture::Mode::Press,out);}
 static void single(const Group&g,Group&out){out={};append(g,false,keygesture::Mode::Press,out);if(g.singleReleased)append(g,false,keygesture::Mode::Release,out);}
 bool press(int id,uint32_t now,uint32_t match,Group&out){out={};if(!match)return false;for(auto&g:groups)if(g.id==id){
  if(g.suppressed)return false;
  bool hasDouble=false;for(auto k:g.bindings)hasDouble|=(k&0x8000)!=0;
  if(g.match!=match){g.waiting=g.down=g.second=g.singleReleased=g.canDouble=false;}
  bool twice=g.waiting&&g.canDouble&&g.match==match&&uint32_t(now-g.at)<=keygesture::Window;
  if(g.waiting&&!twice)single(g,out);
  if(!twice)g.firstReleaseValid=false;
  g.down=true;g.second=twice;g.singleReleased=false;g.match=match;g.waiting=hasDouble&&!twice;g.canDouble=g.waiting;g.at=now;
  if(twice)append(g,true,keygesture::Mode::Press,out);
  else if(!hasDouble)append(g,false,keygesture::Mode::Press,out);
  return !out.actions.empty();
 }return false;}
 bool expired(uint32_t now,uint32_t match,Group&out){for(auto&g:groups)if(g.waiting){if(g.match!=match){g.waiting=g.down=false;continue;}if(uint32_t(now-g.at)>keygesture::Window){g.waiting=false;single(g,out);if(!out.actions.empty())return true;}}return false;}
 void cancel(){for(auto&g:groups)g.waiting=g.down=g.second=g.singleReleased=g.canDouble=g.suppressed=g.firstReleaseValid=false;synchronized=false;}
 template<class Keys> void synchronize(const Keys&keys,uint32_t match){cancel();synchronized=true;for(auto&g:groups){g.match=match;g.suppressed=keys[g.key&255]!=0;}}
 // OS hotkeys still supply keyboard-down notifications. Mouse/bare-modifier
 // commands and all release edges use the existing host sampler; no second
 // injection path, low-level hook, autorepeat or continuous command spam.
 template<class Keys,class Emit> void sample(const Keys&keys,uint32_t now,uint32_t match,Emit emit,bool allowPolledPress=true){
  if(!synchronized){synchronized=true;for(auto&g:groups)if(!g.down){g.match=match;g.suppressed=keys[g.key&255]!=0;}}
  for(auto&g:groups){unsigned vk=g.key&255;bool down=keys[vk]!=0,mods=keygesture::modifiersMatch(keys,g.key);
   if(g.match!=match){g.waiting=g.down=g.second=g.singleReleased=g.canDouble=false;g.match=match;g.suppressed=down;}
   if(g.suppressed){if(!down)g.suppressed=false;continue;}
   if(!mods){if(g.down){
    if(!down){g.down=false;g.firstReleaseValid=false;g.singleReleased=false;g.second=false;}
    else{g.waiting=g.down=g.second=g.singleReleased=g.canDouble=false;g.suppressed=true;}
   }continue;}
   if(g.down&&!down){g.down=false;if(g.second){Group out;append(g,true,keygesture::Mode::Release,out);if(!out.actions.empty())emit(out);g.second=false;}
    else if(g.waiting)g.singleReleased=g.firstReleaseValid=true;
    else{Group out;append(g,false,keygesture::Mode::Release,out);if(!out.actions.empty())emit(out);}
   }else if(down&&!g.down&&allowPolledPress&&keygesture::sampledOnly(g.key)){Group out;if(press(g.id,now,match,out))emit(out);}
  }
 }
};
inline void firstPerGesture(Group&group){
 std::vector<int> actions;std::vector<uint16_t> bindings;
 for(size_t i=0;i<group.actions.size();++i){auto binding=keygesture::normalize(i<group.bindings.size()?group.bindings[i]:group.key);bool found=false;for(auto prior:bindings)found|=prior==binding;if(!found){actions.push_back(group.actions[i]);bindings.push_back(binding);}}
 group.actions=std::move(actions);group.bindings=std::move(bindings);
}
struct Action {int id=0;uint32_t tick=0,match=0;};
struct Pending {
 static constexpr unsigned Capacity=128;Action items[Capacity]{};unsigned head=0,count=0;
 void clear(){head=count=0;}
 void expire(uint32_t now,uint32_t match){
  unsigned kept=0;for(unsigned i=0;i<count;++i){const auto item=items[(head+i)%Capacity];if(item.match==match&&uint32_t(now-item.tick)<=2500)items[(head+kept++)%Capacity]=item;}count=kept;if(!count)head=0;
 }
 bool add(const Group&g,uint32_t tick,uint32_t match){
  expire(tick,match);if(!match||g.actions.size()>Capacity-count)return false;
  for(int id:g.actions)items[(head+count++)%Capacity]={id,tick,match};return true;
 }
 bool pop(uint32_t now,uint32_t match,Action&out){
  while(count){out=items[head];head=(head+1)%Capacity;--count;if(out.match==match&&uint32_t(now-out.tick)<=2500)return true;}return false;
 }
};
}}
