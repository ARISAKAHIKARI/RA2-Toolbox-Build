#pragma once
#include "combat_query_policy.h"
extern "C" int GuardedCombatWeapon(unsigned,unsigned,unsigned*,int*,int*,int*);
extern "C" int GuardedObjectAir(unsigned,int*);
static int objectAirQuery(uint32_t p,int*airborne){
 if(!p||!airborne)return 0;
 const int k=kind(p);if(k<0)return 0;
 if(sh&&sh->nativeCombatQueries)return GuardedObjectAir(p,airborne);
 *airborne=rawObjectAirborne(p,k);return 1;
}
static int combatWeaponQuery(uint32_t p,uint32_t q,uint32_t*weapon,int*selected,int*close,int*airborne){
 if(!p||!q||!weapon||!selected||!close||!airborne)return 0;
 const int k=kind(p),tk=kind(q);const uint32_t type=typeOf(p,k),targetType=typeOf(q,tk);
 if(k<0||tk<0||!type||!targetType||get<bool>(p+0x1C8))return 0;
 if(sh&&sh->nativeCombatQueries)return GuardedCombatWeapon(p,q,weapon,selected,close,airborne);
 planner::Actor source{},target{};
 rawCombatInfo(p,type,k,source,true);rawCombatInfo(q,targetType,tk,target,false);
 source.x=get<int>(p+0x9C);source.y=get<int>(p+0xA0);
 target.x=get<int>(q+0x9C);target.y=get<int>(q+0xA0);
 const auto choice=combatquery::choose(source,target);if(choice.index<0)return 0;
 const auto ptr=rawWeapon(type,get<float>(p+0x150),choice.slot);if(ptr<0x10000)return 0;
 *weapon=ptr;*selected=choice.slot;*close=choice.close;*airborne=(target.flags&planner::Airborne)!=0;
 return 1;
}
