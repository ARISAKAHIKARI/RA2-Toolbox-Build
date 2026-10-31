#pragma once
#include "game_view.h"
#include "scene_lighting.h"
#include "render_material.h"
namespace external { namespace nativedraw {
// GPL-3.0 adaptation of Vera20K draw_state.rs. Values are read from the
// engine-owned Techno, never advanced or written by a presentation viewport.
struct State {bool visible=true,disguised=false;uint32_t type=0,owner=0;int kind=-1;int16_t waterline=0;camera::Material material;};
template<class Read>bool alive(const GameView<Read>&g,uint32_t p,int kind){
 return kind>=0&&g.template get<uint8_t>(p+0x74)&&!g.template get<uint8_t>(p+0x81)&&g.template get<uint8_t>(p+0x90)&&
  (g.template get<int>(p+0x6C)>0||(kind==2&&g.template get<uint8_t>(p+0x3CD)));
}
template<class Read>State capture(Read read,uint32_t p,uint32_t type,int kind,uint32_t observer,int frame){
 GameView<Read> g(read);State out;out.type=type;out.owner=g.template get<uint32_t>(p+0x21C);out.kind=kind;
 const bool owned=g.template get<uint8_t>(p+0x41A)!=0,editor=g.template get<uint8_t>(0xA8ED6B)!=0;
 const int house=g.template get<int>(observer+0x30,-1),cloak=g.template get<int>(p+0x220),progress=g.template get<int>(p+0x224);
 const uint32_t cell=lighting::cellAt(read,g.template get<int>(p+0x9C,-1),g.template get<int>(p+0xA0,-1));
 auto sensed=[&](unsigned offset){return cell&&house>=0&&house<24&&g.template get<uint16_t>(cell+offset+house*2)>0;};
 int character=0;
 if(g.template get<uint8_t>(type+0xC9A)){if(!owned&&!editor)character=5;}
 else if(cloak&&kind!=3&&!editor){
  if(cloak==2){const bool allied=g.allied(out.owner,observer)&&g.allied(observer,out.owner);character=(!g.template get<uint8_t>(0xB73550)||owned||sensed(0x7C)||(g.template get<int>(0xA8B238)!=0&&allied))?3:5;}
  else if(progress>0){int stages=g.template get<int>(g.template get<uint32_t>(0x8871E0)+0x628);int scaled=stages>0?int(std::clamp(int64_t(progress)*256/stages,int64_t(-2147483647),int64_t(2147483647))):0;
   character=scaled<=63?1:scaled<=127?2:scaled<=191?3:owned?3:scaled<255?4:5;}
 }
 out.visible=character<5;if(!out.visible)return out;
 unsigned selector=character==1?1:(character==2||character==3)?2:character==4?(progress?2:1):0;
 if(g.template get<uint8_t>(p+0x270)||g.template get<uint8_t>(p+0x271))selector|=kind==3&&g.template get<uint8_t>(type+0x16B1)?3:2;
 if(kind==1||kind==2){
  const bool active=g.template get<uint8_t>(p+0x1D8)!=0,owner=out.owner==observer;
  const bool blink=assist::remaining(g.template get<int>(p+0x1EC,-1),g.template get<int>(p+0x1F4),frame)!=0;
  const int phase=int32_t(uint32_t(frame)-g.template get<uint32_t>(p+0x1DC)+64u)%256;
  const bool real=blink&&!owner?true:owner&&active?!(phase>=72&&phase<120):!active||!cell||sensed(0xAC);
  if(owner&&active)selector|=((phase>=64&&phase<68)||(phase>=76&&phase<80)||(phase>=112&&phase<116)||(phase>=124&&phase<128))?1:((phase>=68&&phase<76)||(phase>=116&&phase<124))?2:0;
  if(!real){const uint32_t display=g.template get<uint32_t>(p+0x518),vt=g.template get<uint32_t>(display);int displayKind=vt==0x7F6218?2:vt==0x7EB610?1:vt==0x7E2868?0:vt==0x7E4570?3:-1;
   if(displayKind<0){auto terrain=g.vec(0xA8E318,4096);if(std::find(terrain.begin(),terrain.end(),display)!=terrain.end())displayKind=4;}
   if(displayKind>=0){out.disguised=true;out.type=display;out.kind=displayKind;out.owner=g.template get<uint32_t>(p+0x51C);}
  }
 }
 out.material.translucency=selector;
 if(kind==2)out.waterline=g.template get<int16_t>(p+0x3CA);
 return out;
}
}}
