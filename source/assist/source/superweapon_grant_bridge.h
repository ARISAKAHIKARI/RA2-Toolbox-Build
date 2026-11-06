#include "../../shared/source/debug_entry_checks.h"
#pragma once
// Explicit experimental local simulation action. This is not a network Grant
// event: every peer would need matching state. Off stops calls; native permanent
// grants already made are retained for this match, exactly like map action 34.
struct SuperweaponGrantFrame {
 uint32_t epoch=0,house=0,houseID=0,scenario=0;int frame=-1;
};
static SuperweaponGrantFrame superweaponGrantFrame;
static void superweaponGrantReset(){
 superweaponGrantFrame={};if(!sh)return;
 sh->superweaponGrantStatus=sh->superweaponGrant&&!sh->stop?2:0;
 sh->superweaponGrantCount=0;sh->superweaponGrantFrame=-1;
}
template<class T>static bool superweaponGrantField(uint32_t at,T&value){
 return at>=0x10000&&uint64_t(at)+sizeof(T)<0x80000000ull&&read(at,&value,sizeof value);
}
static bool superweaponGrantSignatures(){
 struct Probe{uint32_t address,bytes,hash;};
 static constexpr Probe probes[]={{0x6E1C40,117,0x8B1ECC5B},{0x6CB560,12,0x5DC034C2},
  {0x6A6300,12,0xB32476A7},{0x6ABC60,12,0xC602F953},{0x6A60A0,12,0x049FEAAA}};
 for(const auto&p:probes){uint8_t bytes[117]{};const bool readable=read(p.address,bytes,p.bytes);
  uint32_t hash=2166136261u;for(unsigned i=0;i<p.bytes;++i)hash=(hash^bytes[i])*16777619u;
  if(!debugentries::allow(debugentries::Kind::Call,p.address,readable,hash==p.hash))return false;
 }return true;
}
static bool superweaponGrantWantsInputSample(const MSG*msg){
 return sh&&sh->superweaponGrant&&!sh->stop&&!fault&&msg&&nativePlacementWindow(msg->hwnd);
}
static void superweaponGrantTick(uint32_t house,int frame,DWORD now){
 if(!sh||sh->stop||!sh->superweaponGrant){superweaponGrantReset();return;}
 if(fault||snap.status!=1||snap.commandStatus!=1||!simulationReady()||frame<0||frame>=INT32_MAX||
    DWORD(now-sh->heartbeat)>1500||house!=lastHouse||!match){sh->superweaponGrantStatus=2;return;}
 uint32_t current=0,scenario=0,houseID=0;uint8_t defeated=0,loser=0,observer=0;
 if(!superweaponGrantField(0xA83D4C,current)||current!=house||
    !superweaponGrantField(0xA8B230,scenario)||scenario<0x10000||
    !superweaponGrantField(house+0x10,houseID)||!superweaponGrantField(house+0x1F5,defeated)||
    !superweaponGrantField(house+0x1F6,loser)||!superweaponGrantField(house+0x1F8,observer)||
    defeated||loser||observer||house==get<uint32_t>(0xAC1198)){sh->superweaponGrantStatus=2;return;}
 auto&stamp=superweaponGrantFrame;
 if(stamp.epoch!=match||stamp.house!=house||stamp.houseID!=houseID||stamp.scenario!=scenario||frame<stamp.frame)
  stamp={match,house,houseID,scenario,-1};
 if(frame==stamp.frame)return;
 stamp.frame=frame;sh->superweaponGrantFrame=frame;sh->superweaponGrantCount=0;
 if(!superweaponGrantSignatures()){sh->superweaponGrantStatus=-1;return;}
 const Vec supers=vec(house+0x254,256);int count=-1;
 if(!superweaponGrantField(house+0x264,count)||count<0||count>256||supers.count!=count||
    (count&&supers.items<0x10000)){sh->superweaponGrantStatus=-2;return;}
 // The verified original action reads only this Value field. No vtable or
 // constructor-dependent action field is used in its complete 117-byte body.
 struct ActionValue {uint8_t unused[0x90]{};int value=0;} action;
 static_assert(offsetof(ActionValue,value)==0x90&&sizeof(ActionValue)==0x94,"native map action Value ABI");
 vanilla::Cell cell{0,0};
 for(int i=0;i<count;++i){
  uint32_t sw=0,type=0,owner=0,id=0,typeID=0,again=0,items=0;int typeIndex=-1;
  if(!superweaponGrantField(supers.items+uint32_t(i)*4,sw)||sw<0x10000||
     !superweaponGrantField(sw+0x10,id)||!superweaponGrantField(sw+0x28,type)||type<0x10000||
     !superweaponGrantField(sw+0x2C,owner)||owner!=house||!superweaponGrantField(type+0x10,typeID)||
     !superweaponGrantField(type+0x98,typeIndex)||typeIndex!=i){sh->superweaponGrantStatus=-2;return;}
  // Revalidate immediately before the side effect, never a cached pointer.
  if(!sh->superweaponGrant||sh->stop||!simulationReady()||
     !superweaponGrantField(0xA83D4C,current)||current!=house||
     !superweaponGrantField(0xA8B230,current)||current!=scenario||
     !superweaponGrantField(house+0x10,current)||current!=houseID||
     !superweaponGrantField(house+0x258,items)||items!=supers.items||
     !superweaponGrantField(house+0x264,typeIndex)||typeIndex!=count||
     !superweaponGrantField(items+uint32_t(i)*4,again)||again!=sw||
     !superweaponGrantField(sw+0x10,current)||current!=id||
     !superweaponGrantField(sw+0x28,current)||current!=type||
     !superweaponGrantField(sw+0x2C,current)||current!=house||
     !superweaponGrantField(type+0x10,current)||current!=typeID){sh->superweaponGrantStatus=-2;return;}
  action.value=i;
  const bool accepted=call<char>(0x6E1C40,uint32_t(uintptr_t(&action)),house,uint32_t(0),uint32_t(0),&cell)!=0;
  uint8_t present=0,canHold=1;
  if(!accepted||!superweaponGrantField(sw+0x6D,present)||!present||
     !superweaponGrantField(sw+0x60,canHold)||canHold){sh->superweaponGrantStatus=-3;return;}
  ++sh->superweaponGrantCount;
 }
 sh->superweaponGrantStatus=1;
}
