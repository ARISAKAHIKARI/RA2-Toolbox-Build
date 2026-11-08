#pragma once
#include "game_view.h"
#include "scene_lighting.h"
#include "render_material.h"
#include "projection.h"
#include "native_foot_depth.h"
#include "optional_read.h"
namespace external { namespace nativedraw {
// GPL-3.0 adaptation of Vera20K draw_state.rs. Values are read from the
// engine-owned Techno, never advanced or written by a presentation viewport.
struct State {bool visible=true,disguised=false;uint32_t type=0,owner=0;int kind=-1;int16_t waterline=0;camera::Material material;};
// CnCNet yr-patches0318b1f: adopt its observer/detached-alliance policy only
// when the live branch resolves to the documented cloak patch instructions.
// An unrelated detour is not evidence that a different visibility rule exists.
template<class Read>bool cncCloakPolicy(Read read,uint32_t site){
 std::array<uint8_t,5>branch{};if(!optionalread::bytes(read,site,branch.data(),branch.size())||branch[0]!=0xE9)return false;int32_t delta=0;std::memcpy(&delta,branch.data()+1,4);int64_t target=int64_t(site)+5+delta;if(target<0x10000||target>0x7FFFFFF0)return false;
 const std::array<uint8_t,12>expected{{0x8B,0x15,0x4C,0x3D,0xA8,0,0x39,0x15,0x98,0x11,0xAC,0}};std::array<uint8_t,12>code{};return optionalread::bytes(read,uint32_t(target),code.data(),code.size())&&code==expected;
}
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
 const bool cnc=cncCloakPolicy(read,kind==3?0x45455B:0x703A09);
 const bool observing=observer&&observer==g.template get<uint32_t>(0xAC1198);
 auto ownerAllied=[&]{return out.owner==observer||(house>=0&&house<32&&(g.template get<uint32_t>(out.owner+0x5788)&(1u<<house)));};
 // Native70396B / 454552 test the whole Game::hWnd DWORD. Reading its low
 // byte made some perfectly valid HWNDs masquerade as a headless client.
 auto shadowVisible=[&]{const bool allied=g.allied(out.owner,observer)&&ownerAllied();return !g.template get<uint32_t>(0xB73550,1)||owned||sensed(0x7C)||(cnc?(observing||ownerAllied()):(g.template get<int>(0xA8B238)!=0&&allied));};
 if(g.template get<uint8_t>(type+0xC9A)){if(!owned&&!editor)character=5;}
 else if(cloak&&!editor){
  if(cloak==2)character=shadowVisible()?3:5;
  else if(progress>0){int stages=g.template get<int>(g.template get<uint32_t>(0x8871E0)+0x628);int scaled=stages>0?int(std::clamp(int64_t(progress)*256/stages,int64_t(-2147483647),int64_t(2147483647))):0;
   character=scaled<=63?1:scaled<=127?2:scaled<=191?3:owned?3:scaled<255?4:5;}
 }
 if(kind==3){int stage=g.template get<int8_t>(p+0x6ED);if(stage)character=stage<=10?(stage>5?2:1):shadowVisible()?3:5;}
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
 out.material.nativeZ=true;out.material.nativeZAdjust=-screenHeight(g.template get<int>(p+0xA4),projectionZScale(read))-2;
 const bool voxel=g.template get<uint8_t>(out.type+0x236)!=0;
 if(kind>=0&&kind<=2){nativefoot::Sample<Read>sample(read,p,frame);auto depth=sample.capture(p,type,kind,frame,voxel);out.material.nativeZAdjust=depth.adjust;out.material.bridgeSplit=depth.split;out.material.shpFlat=depth.shpFlat;}
 out.material.displace=character==4&&(voxel||(kind==3&&g.template get<uint8_t>(type+0x16C5)));
 if(out.material.displace){float raw=g.template get<float>(p+0x24C);int32_t chopped=std::isfinite(raw)&&double(raw)>=-2147483648.&&double(raw)<2147483648.?int32_t(raw):int32_t(0x80000000u);
  out.material.cloakWords=int32_t(g.template get<uint32_t>(p+0x10)+uint32_t(chopped))%400;}
 std::array<int,6>format{};if(optionalread::bytes(read,0x8A0DD0,format.data(),sizeof format))out.material.rgb555=format==std::array<int,6>{{10,3,0,3,5,3}};
 if(kind==2)out.waterline=g.template get<int16_t>(p+0x3CA);
 return out;
}
}}
