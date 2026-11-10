#pragma once
#include "sell_packet.h"
static bool sellHumanHouse(uint32_t owner){return owner>=0x10000&&(get<uint8_t>(owner+0x1EC)||get<uint8_t>(owner+0x1ED));}
static bool executeSinglePlayerAISell(Event&packet,uint32_t object){
 if(!simulationReady()||!eventdelivery::isSingleplayer(get<int>(0xA8B238))||!object||!worldObjects.contains(object)||!live(object)||kind(object)!=3)return false;
 const uint32_t owner=get<uint32_t>(object+0x21C),id=get<uint32_t>(object+0x10);
 if(!playableHouse(owner)||sellHumanHouse(owner)||packet.kind!=0x16||packet.house!=get<int>(owner+0x30)||packet.payload[4]!=0x34||!id||memcmp(packet.payload,&id,4))return false;
 static constexpr uint8_t head[]={0x81,0xEC,0x70,3,0,0,0x33,0xC0,0x53,0x55,0x56,0x8B,0xF1};
 static constexpr uint8_t ownerCheck[]={0x8B,0x8F,0x1C,2,0,0,0x0F,0xBE,0x56,2,0x39,0x51,0x30};
 if(!debugentries::pattern(debugentries::Kind::Call,0x4C6CB0,head,sizeof head,read)||
  !debugentries::pattern(debugentries::Kind::Call,0x4C6F3B,ownerCheck,sizeof ownerCheck,read)||get<uint32_t>(0x4C8114+21*4)!=0x4C6F12)return false;
 // Single-player AI Houses are skipped by the ordinary House scheduler. Run
 // the complete native Sell receiver once, preserving its Owner/type checks.
 // Never take this path in multiplayer or mutate House human-control flags.
 call<void>(0x4C6CB0,uint32_t(uintptr_t(&packet)));return true;
}
