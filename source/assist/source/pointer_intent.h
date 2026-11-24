#pragma once
#include <stdint.h>
#include "../../shared/source/key_gesture.h"
namespace assist { namespace pointerinput {
constexpr int MaxSources=1024;
enum Operation { Enhanced=1, AttackMove=2, Sell=3, FollowNative=4, Cancel=5, AirWaypoint=6, C4Test=7, PlayerSelect=8, PlayerOrder=9 };
enum Binding { Advanced=1, Magnetron=2, CS=4, Enter=8, AirFirst=16, AirSecond=32, Control=64, Shift=128, RightClick=1024, SellDrainedInput=2048, SellVehicleInput=4096, PerFeatureMouse=8192, BatchStart=16384, BatchNext=32768, LeftClick=65536, NativeForced=131072, EntrySabotage=262144 };
inline bool entryClick(int operation,uint32_t bindings){
 const uint32_t modifiers=Shift|RightClick|LeftClick|PerFeatureMouse|BatchStart|BatchNext;
 return operation==Enhanced&&(bindings&~modifiers)==Enter;
}
inline bool clickFresh(int operation,uint32_t bindings,bool local,bool extended,
                       uint32_t created,uint32_t now,int sourceFrame,int frame){
 // Native click admission uses wall time, not a fixed number of game
 // frames. At 240 FPS eight frames elapse in ~33 ms before one IPC hop.
 // The caller still validates match, input serial, House, viewport and UID.
 (void)operation;(void)bindings;(void)local;
 return sourceFrame>=0&&frame>=sourceFrame&&uint32_t(now-created)<=(extended?1500u:150u);
}
inline int entryMission(uint32_t bindings){return bindings&EntrySabotage?17:8;}
inline const wchar_t* entryLabel(uint32_t mode){return mode==1?L"爆破任务＋建筑格":L"占领任务＋建筑格";}
// Physical mouse capture uses 1=middle, 2/3=side buttons, 4=right release.
// Preserve native right-click handling; configured right orders execute later.
// Zero is a mouse-only fallback, never an implicit right-click modifier.
inline bool matchesHold(const uint8_t*keys,uint32_t key,int operation){
 const unsigned vk=key&255;
 if(!vk)return (operation==Enhanced||operation==AttackMove||operation==Sell)&&!keys[16]&&!keys[17]&&!keys[18];
 return keygesture::matchesHold(keys,key);
}
struct Bindings {uint32_t advanced,magnetron,cs,enter,airFirst,airSecond,disk,sell,enabled;};
// IPC v115: low 16 bits contain feature-enable flags; upper 16 bits store
// the optional attack-move hold gesture. Preserve shared structure size.
constexpr uint32_t FeatureEnableMask=0xffffu;
inline uint32_t bindingOptions(uint32_t flags,uint32_t attackHold){return (flags&FeatureEnableMask)|((attackHold&0xffffu)<<16);}
inline bool attackHoldMatches(const uint8_t*keys,uint32_t options){const uint32_t key=options>>16;return !(key&255u)||keygesture::matchesHold(keys,key);}
inline uint32_t resolveBindings(const uint8_t*keys,int operation,const Bindings&c){
 uint32_t out=0,explicitGround=0;
 const uint32_t values[]={c.advanced,c.magnetron,c.cs},bits[]={Advanced,Magnetron,CS};
 for(int i=0;i<3;++i)if((c.enabled&bits[i])&&matchesHold(keys,values[i],operation)){
  out|=bits[i];if(values[i]&255)explicitGround|=bits[i];
 }
 // An explicitly held key wins over mouse-only fallbacks in the same family.
 if(explicitGround)out=explicitGround;
 if((c.enabled&Enter)&&matchesHold(keys,c.enter,operation))out|=Enter;
 const bool first=(c.enabled&AirFirst)&&matchesHold(keys,c.airFirst,operation);
 const bool second=(c.enabled&AirSecond)&&matchesHold(keys,c.airSecond,operation);
 if(second&&(!first||(c.airSecond&&!c.airFirst)))out|=AirSecond;
 else if(first)out|=AirFirst;
 // Control is the existing protocol authorization bit for these two actions;
 // the Runtime still validates its operation, feature switch and target.
 if((operation==Enhanced&&(c.enabled&256)&&matchesHold(keys,c.disk,operation))||
    (operation==Sell&&(c.enabled&512)&&matchesHold(keys,c.sell,operation)))out|=Control;
 if(keys[16])out|=Shift;
 return out;
}
// Per-feature physical buttons. Existing operation precedence is retained;
// this does not introduce configurable priorities or a second input channel.
enum MouseFeature { MouseAdvanced,MouseMagnetron,MouseCS,MouseEnter,MouseAirFirst,MouseAirSecond,MouseDisk,MouseAttack,MouseSellDrained,MouseSellVehicle,MouseCount };
inline uint32_t mouseDefault(int slot){return slot<MouseAttack?1:slot==MouseAttack?2:3;}
inline const wchar_t* mouseSetting(int slot){
 const wchar_t*names[]={L"AdvancedButton",L"MagnetronButton",L"CSButton",L"EnterButton",L"AirFirstButton",L"AirSecondButton",L"DiskButton",L"AttackMoveButton",L"SellDrainedButton",L"SellVehicleButton"};return names[slot];
}
struct MouseRoute {int operation;uint32_t bindings;};
inline MouseRoute resolveMouse(const uint8_t*keys,int physical,const uint32_t*buttons,Bindings config,uint32_t saleEnabled){
 if(physical==Cancel)return {Cancel,0};
 if(physical<1||physical>6||physical==5)return {};
 const uint32_t bits[]={Advanced,Magnetron,CS,Enter,AirFirst,AirSecond,256};
 uint32_t enabled=0;for(int i=0;i<MouseAttack;++i)if(buttons[i]==uint32_t(physical))enabled|=bits[i];
 const auto allEnabled=config.enabled;config.enabled&=enabled;
 uint32_t bindings=resolveBindings(keys,Enhanced,config);
 uint32_t follow=physical==FollowNative?RightClick:physical==6?LeftClick:0;
 if(bindings&~Shift)return {Enhanced,bindings|follow|PerFeatureMouse};
 if(buttons[MouseAttack]==uint32_t(physical)&&attackHoldMatches(keys,allEnabled))return {AttackMove,(keys[16]?uint32_t(Shift):0)|follow|PerFeatureMouse};
 uint32_t sale=0;
 if(buttons[MouseSellDrained]==uint32_t(physical))sale|=SellDrainedInput;
 if(buttons[MouseSellVehicle]==uint32_t(physical))sale|=SellVehicleInput;
 sale&=saleEnabled;config.enabled=allEnabled;
 if(sale&&(resolveBindings(keys,Sell,config)&Control))return {Sell,Control|sale|follow|PerFeatureMouse|(keys[16]?uint32_t(Shift):0)};
 return {(physical==FollowNative||physical==6)?FollowNative:0,PerFeatureMouse|follow|(keys[16]?uint32_t(Shift):0)};
}
inline int resolveMouseMatches(const uint8_t*keys,int physical,const uint32_t*buttons,const Bindings&c,uint32_t saleEnabled,bool all,MouseRoute*out){
 if(!all||physical==Cancel){auto one=resolveMouse(keys,physical,buttons,c,saleEnabled);if(one.operation){out[0]=one;return 1;}return 0;}
 if(physical<1||physical>6||physical==5)return 0;
 const uint32_t bits[]={Advanced,Magnetron,CS,Enter,AirFirst,AirSecond,Control};
 const uint32_t holds[]={c.advanced,c.magnetron,c.cs,c.enter,c.airFirst,c.airSecond,c.disk};
 const uint32_t enables[]={Advanced,Magnetron,CS,Enter,AirFirst,AirSecond,256};
 const uint32_t common=PerFeatureMouse|(physical==FollowNative?uint32_t(RightClick):physical==6?uint32_t(LeftClick):0)|(keys[16]?uint32_t(Shift):0);
 int count=0;
 for(int j=0;j<MouseCount;++j){if(buttons[j]!=uint32_t(physical))continue;
  if(j<MouseAttack){if((c.enabled&enables[j])&&matchesHold(keys,holds[j],Enhanced))out[count++]={Enhanced,bits[j]|common};}
  else if(j==MouseAttack){if(attackHoldMatches(keys,c.enabled))out[count++]={AttackMove,common};}
  else {uint32_t sale=j==MouseSellDrained?SellDrainedInput:SellVehicleInput;
   if((c.enabled&512)&&(saleEnabled&sale)&&matchesHold(keys,c.sell,Sell))out[count++]={Sell,Control|sale|common};}
 }
 if(!count&&(physical==FollowNative||physical==6))out[count++]={FollowNative,common};return count;
}
// Keyboard-only point commands use the already sampled key edge serials.
 // Legacy/hold bindings fire once on the qualified down edge, not every frame.
struct KeyOnlyEdges {
 uint32_t serial[MouseCount]{};
 bool armed[MouseCount]{};
 uint32_t match=0,revision=0;bool ready=false;
 int resolve(const uint8_t*keys,const uint32_t*edges,const uint32_t*buttons,
             const Bindings&c,uint32_t saleEnabled,bool all,bool active,
             uint32_t epoch,uint32_t settings,MouseRoute*out){
  const uint32_t holds[]={c.advanced,c.magnetron,c.cs,c.enter,c.airFirst,c.airSecond,c.disk,c.enabled>>16,c.sell,c.sell};
  const uint32_t flags[]={Advanced,Magnetron,CS,Enter,AirFirst,AirSecond,256,0,512,512};
  const uint32_t bits[]={Advanced,Magnetron,CS,Enter,AirFirst,AirSecond,Control,0,Control|SellDrainedInput,Control|SellVehicleInput};
  const bool reset=!active||!ready||match!=epoch||revision!=settings;
  match=epoch;revision=settings;ready=active;int count=0;
  for(int i=0;i<MouseCount;++i){
   const uint32_t key=holds[i],vk=key&255,edge=edges[vk];
   const bool available=active&&!buttons[i]&&vk>=7&&(!flags[i]||(c.enabled&flags[i]))&&
       (i<MouseSellDrained||(saleEnabled&(i==MouseSellDrained?SellDrainedInput:SellVehicleInput)));
   const bool fresh=serial[i]!=edge;serial[i]=edge;
   if(reset||!available){armed[i]=false;continue;}
   const bool qualifies=keygesture::matchesHold(keys,(key&~keygesture::ModeMask)|uint32_t(keygesture::Mode::Hold));
   const bool release=keygesture::mode(key)==keygesture::Mode::Release;
   bool fire=false;
   if(fresh){armed[i]=release&&qualifies;fire=!release&&qualifies;}
   if(release&&armed[i]&&!keys[vk]){armed[i]=false;fire=keygesture::modifiersMatch(keys,key);}
   if(fire&&(all||!count))out[count++]={i<MouseAttack?Enhanced:i==MouseAttack?AttackMove:Sell,bits[i]|PerFeatureMouse};
  }
  return count;
 }
};
struct NativePolicy {uint32_t match,enabled,overrideNative;Bindings bindings;uint32_t buttons[MouseCount];};
struct SuppressedClick {uint32_t match,tick,physical;int x,y;};
struct Source { uint32_t id,pointer; };
struct Intent {
 uint32_t tick,match,house,bindings,inputSerial;int frame,operation,x,y,width,height,cameraX,cameraY,count;
 Source sources[MaxSources];
};
// One host writer / one Runtime reader. Acknowledgement never comes from EXE.
struct Mailbox { volatile uint32_t request,ack,cancelSerial;Intent intent;int result,accepted,rejected;uint32_t resolvedCell,resolvedTarget;int resolvedZ;uint32_t sourceTokens[MaxSources],sourceAccepted[(MaxSources+31)/32]; };
}}


