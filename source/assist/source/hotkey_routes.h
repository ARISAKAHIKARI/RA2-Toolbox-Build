#pragma once
#include <stdint.h>
#include <vector>
#include "../../shared/source/key_gesture.h"
namespace assist { namespace hotkeyroute {
inline uint16_t canonical(uint16_t key){return key&0x7ff;}
struct Group {int id=0;uint16_t key=0;std::vector<int> actions;std::vector<uint16_t> bindings;uint32_t at=0,match=0;bool waiting=false,down=false,second=false,singleReleased=false,canDouble=false,suppressed=false,firstReleaseValid=false;
 // A sampled press may be followed by its delayed WM_HOTKEY, even after the
 // key was released. Keep its timestamp beyond release to consume that
 // notification once. A context fence also outlives cancelled gestures.
 uint32_t edgeTick=0,fenceTick=0;bool edgeTimed=false,fenceTimed=false;
 uint32_t lastUpTick=0,pollFloor=0,pollReleaseTick=0;bool upTimed=false,pollPending=false,pollReleased=false;
};
struct Registry {
 std::vector<Group> groups;bool synchronized=false;uint32_t lastSampleTick=0;bool sampleTimed=false;
 // Context admission outlives gesture cancellation while another game input
 // owns the pointer. Formation WM handoff must reject an earlier focus/match
 // epoch without treating every preview-owned sample as a new context fence.
 uint32_t contextMatch=0,contextFence=0;bool contextActive=false;
 void observeContext(bool active,uint32_t match,uint32_t now){
  if(!active||!match){contextActive=false;contextFence=now;return;}
  if(!contextActive||contextMatch!=match)contextFence=now;
  contextActive=true;contextMatch=match;
 }
 bool contextMessageFresh(uint32_t eventTick,uint32_t now,uint32_t match,uint32_t maximumAge)const{
  return contextActive&&contextMatch==match&&later(eventTick,contextFence)&&uint32_t(now-eventTick)<=maximumAge;
 }
 bool sampleFresh(uint32_t now)const{return sampleTimed&&uint32_t(now-lastSampleTick)<=150;}
 void clear(){groups.clear();synchronized=false;sampleTimed=false;contextActive=false;}
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
 static bool later(uint32_t tick,uint32_t prior){const uint32_t elapsed=tick-prior;return elapsed&&elapsed<0x80000000u;}
 // eventTick is GetMessageTime, not the time at which the host finally
 // dequeues the notification. A later observed physical edge supersedes an
 // older queued message; never replay it as a new tap or a double tap.
 bool message(int id,uint32_t eventTick,uint32_t now,uint32_t match,Group&out){
  out={};if(!match||!synchronized||uint32_t(now-eventTick)>2500)return false;
  for(auto&g:groups)if(g.id==id){
   if(g.match!=match){
    g.waiting=g.down=g.second=g.singleReleased=g.canDouble=g.suppressed=g.firstReleaseValid=false;
    g.match=match;g.edgeTimed=g.pollPending=g.pollReleased=g.upTimed=false;g.fenceTick=now;g.fenceTimed=true;synchronized=false;sampleTimed=false;contextActive=false;return false;
   }
   if(g.suppressed||(g.fenceTimed&&!later(eventTick,g.fenceTick)))return false;
   if(g.edgeTimed&&!later(eventTick,g.edgeTick)){
    // An old notification from before this physical press must not consume
    // the acknowledgement belonging to a newer sampled press.
    if(g.pollPending&&(eventTick==g.edgeTick||later(eventTick,g.pollFloor)))g.pollPending=false;
    return false;
   }
   if(g.pollPending){
    // GetMessageTime records queue insertion: WM_HOTKEY may be stamped a
    // little AFTER GetAsyncKeyState first observed the same press. Pair its
    // first notification rather than inventing a second physical edge.
    const bool samePress=!g.pollReleased||!later(eventTick,g.pollReleaseTick);
    g.pollPending=false;
    if(samePress){g.edgeTick=eventTick;g.edgeTimed=true;return false;}
   }
   g.edgeTick=eventTick;g.edgeTimed=true;
   return press(id,eventTick,match,out);
  }
  return false;
 }
 bool expired(uint32_t now,uint32_t match,Group&out){for(auto&g:groups)if(g.waiting){if(g.match!=match){g.waiting=g.down=false;continue;}if(uint32_t(now-g.at)>keygesture::Window){g.waiting=false;single(g,out);if(!out.actions.empty())return true;}}return false;}
 void cancel(){for(auto&g:groups){g.waiting=g.down=g.second=g.singleReleased=g.canDouble=g.suppressed=g.firstReleaseValid=false;g.edgeTimed=g.fenceTimed=g.upTimed=g.pollPending=g.pollReleased=false;}synchronized=false;sampleTimed=false;contextActive=false;}
 void cancel(uint32_t now,bool preserveContext=false){const bool active=contextActive;cancel();contextActive=preserveContext&&active;for(auto&g:groups){g.fenceTick=now;g.fenceTimed=true;}}
 template<class Keys> void synchronize(const Keys&keys,uint32_t match){cancel();synchronized=true;for(auto&g:groups){g.match=match;g.suppressed=keys[g.key&255]!=0;}}
 template<class Keys> void synchronize(const Keys&keys,uint32_t match,uint32_t now){synchronize(keys,match);lastSampleTick=now;sampleTimed=true;observeContext(true,match,now);for(auto&g:groups){g.fenceTick=now;g.fenceTimed=true;}}
 // Every command key has a physical-edge fallback. OS notifications retain
 // short presses between samples; message() and this sampler share one edge
 // watermark, so neither notification order can produce duplicate commands.
 template<class Keys,class Emit> void sample(const Keys&keys,uint32_t now,uint32_t match,Emit emit,bool allowPolledPress=true){
  lastSampleTick=now;sampleTimed=true;observeContext(true,match,now);
  if(!synchronized){synchronized=true;for(auto&g:groups)if(!g.down){g.match=match;g.suppressed=keys[g.key&255]!=0;g.fenceTick=now;g.fenceTimed=true;}}
  for(auto&g:groups){unsigned vk=g.key&255;bool down=keys[vk]!=0;
   // Command bindings encode Ctrl/Alt/Shift only. Polling must not turn a
   // Windows shortcut into a game command; do not change shared hold rules.
   const bool windowsPrefix=(keys[91]&&vk!=91)||(keys[92]&&vk!=92);
   bool mods=!windowsPrefix&&keygesture::modifiersMatch(keys,g.key);
   if(g.match!=match){g.waiting=g.down=g.second=g.singleReleased=g.canDouble=g.firstReleaseValid=false;g.match=match;g.suppressed=down;g.edgeTimed=g.upTimed=g.pollPending=g.pollReleased=false;g.fenceTick=now;g.fenceTimed=true;}
   if(!down){g.lastUpTick=now;g.upTimed=true;if(g.pollPending&&!g.pollReleased){g.pollReleaseTick=now;g.pollReleased=true;}}
   if(g.suppressed){if(!down){g.suppressed=false;g.fenceTick=now;g.fenceTimed=true;}continue;}
   if(!mods){
    if(down){g.waiting=g.down=g.second=g.singleReleased=g.canDouble=g.firstReleaseValid=false;g.suppressed=true;g.fenceTick=now;g.fenceTimed=true;}
    else if(g.down){g.down=false;g.firstReleaseValid=false;g.singleReleased=false;g.second=false;}
    continue;
   }
   if(g.down&&!down){g.down=false;if(g.second){Group out;append(g,true,keygesture::Mode::Release,out);if(!out.actions.empty())emit(out);g.second=false;}
    else if(g.waiting)g.singleReleased=g.firstReleaseValid=true;
    else{Group out;append(g,false,keygesture::Mode::Release,out);if(!out.actions.empty())emit(out);}
   }else if(down&&!g.down&&allowPolledPress){
    g.pollFloor=g.upTimed?g.lastUpTick:g.fenceTimed?g.fenceTick:now;
    g.pollPending=!keygesture::sampledOnly(g.key);g.pollReleased=false;
    g.edgeTick=now;g.edgeTimed=true;Group out;if(press(g.id,now,match,out))emit(out);
   }
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
 // Local Host actions can pass an unrelated busy game mailbox. Remove only
 // the selected item; deferred commands keep their original order and age.
 template<class Ready> bool popReady(uint32_t now,uint32_t match,Action&out,Ready ready){
  expire(now,match);
  for(unsigned i=0;i<count;++i){const auto item=items[(head+i)%Capacity];if(!ready(item.id))continue;
   out=item;for(unsigned j=i+1;j<count;++j)items[(head+j-1)%Capacity]=items[(head+j)%Capacity];
   --count;if(!count)head=0;return true;
  }return false;
 }
 bool pop(uint32_t now,uint32_t match,Action&out){
  while(count){out=items[head];head=(head+1)%Capacity;--count;if(out.match==match&&uint32_t(now-out.tick)<=2500)return true;}return false;
 }
};
}}
