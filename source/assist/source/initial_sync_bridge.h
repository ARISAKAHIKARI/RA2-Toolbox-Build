#pragma once
#include "initial_sync_gate.h"
#include "../../shared/source/multiplayer_patch_gate.h"
static initialsync::Gate initialSyncGate;
static DWORD signatureRetryAt;
static void initialSyncReset(){initialSyncGate.reset();multiplayerpatch::clear();checked=good=false;signatureRetryAt=0;}
static bool initialSyncTick(int mode,int frame){
 if(eventdelivery::isSingleplayer(mode))return true;
 initialsync::Identity identity{match,0,0,lastHouse,0,mode};uint8_t oos=0;
 if(!read(0xA8B230,&identity.scenario,4)||!read(0xA8022C,&identity.items,4)||
    !read(lastHouse+0x10,&identity.id,4)||!read(0xA8B8C2,&oos,1)){multiplayerpatch::clear();return false;}
 Vec houses=vec(0xA80228,32);uint32_t peers=0,observer=0;int local=-1,players=0;
 if(houses.items!=identity.items||!read(lastHouse+0x30,&local,4)||!read(0xAC1198,&observer,4)||
    !read(0xA8B54C,&players,4)||players<1||players>8){
  multiplayerpatch::clear();return false;
 }
 bool foundLocal=false;
 for(int i=0;i<houses.count;++i){uint32_t house=0;int index=-1;uint8_t human=0,defeated=0;
  if(!read(houses.items+i*4,&house,4)||house<0x10000||!read(house+0x30,&index,4)||index!=i||
     !read(house+0x1EC,&human,1)||!read(house+0x1F5,&defeated,1)){multiplayerpatch::clear();return false;}
  if(house==lastHouse&&i==local)foundLocal=true;
  if(house!=lastHouse&&house!=observer&&human&&!defeated)peers|=1u<<i;
 }
 if(!foundLocal||(!peers&&players>1&&!initialSyncGate.hasProof(identity,frame))){multiplayerpatch::clear();return false;}
 const bool ready=initialSyncGate.update(identity,frame,peers,oos!=0,read);
 if(!ready)multiplayerpatch::clear();return ready;
}
static void initialSyncApprove(int mode,int frame){
 if(eventdelivery::isMultiplayer(mode)){const auto&i=initialSyncGate.identity;
  multiplayerpatch::approve(i.scenario,i.items,i.house,i.id,mode,frame);
 }
}
