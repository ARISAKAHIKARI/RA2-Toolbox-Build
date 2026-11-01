#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdio>
#include <type_traits>
#include "../source/combat_query_policy.h"

using namespace assist;
static Shared options;
static Shared* sh=&options;
static planner::Actor shooter,victim;
static int nativeWeaponCalls,nativeAirCalls;
static constexpr uint32_t Shooter=0x100000,Victim=0x200000,WeaponPointer=0x300000;
static int kind(uint32_t p){return p==Shooter?shooter.kind:p==Victim?victim.kind:-1;}
static uint32_t typeOf(uint32_t p,int k){return k>=0?p+0x10000:0;}
template<class T>static T get(uint32_t p){
 if constexpr(std::is_same_v<T,bool>)return false;
 if(p==Shooter+0x9C)return T(shooter.x);if(p==Shooter+0xA0)return T(shooter.y);
 if(p==Victim+0x9C)return T(victim.x);if(p==Victim+0xA0)return T(victim.y);
 return T{};
}
static void rawCombatInfo(uint32_t p,uint32_t,int,planner::Actor&a,bool){a=p==Shooter?shooter:victim;}
static uint32_t rawWeapon(uint32_t,float,int){return WeaponPointer;}
static bool rawObjectAirborne(uint32_t p,int){return ((p==Shooter?shooter:victim).flags&planner::Airborne)!=0;}
extern "C" int GuardedCombatWeapon(unsigned,unsigned,unsigned*w,int*slot,int*close,int*air){
 ++nativeWeaponCalls;*w=WeaponPointer+1;*slot=17;*close=0;*air=1;return 1;
}
extern "C" int GuardedObjectAir(unsigned,int*air){++nativeAirCalls;*air=1;return 1;}
#include "../source/combat_query_bridge.h"

int main(){
 using namespace planner;
 int checks=0;
 auto check=[&](bool ok){assert(ok);++checks;};
 shooter.kind=2;victim.kind=2;victim.armor=0;victim.x=300;victim.y=400;
 auto&w=shooter.weapons[0];w.range=500;w.slot=0;w.armorMask=w.selectMask=1;w.flags=GroundWeapon;
 check(combatquery::choose(shooter,victim).close); // inclusive range boundary
 victim.x=301;check(!combatquery::choose(shooter,victim).close);victim.x=300;
 w.minimum=501;check(combatquery::choose(shooter,victim).index==-1);w.minimum=500;
 check(combatquery::choose(shooter,victim).close);victim.x=0;victim.y=499;
 check(!combatquery::choose(shooter,victim).close);w.minimum=0;
 victim.z=100;check(!combatquery::choose(shooter,victim).close);
 shooter.flags|=Airborne;check(combatquery::choose(shooter,victim).close);shooter.flags=0;victim.z=0;
 auto&q=shooter.weapons[1];q=w;q.range=1000;q.slot=7;q.flags=AirWeapon;
 victim.flags=Airborne;victim.y=1100;shooter.airRangeBonus=100;
 auto selected=combatquery::choose(shooter,victim);check(selected.index==1&&selected.slot==7&&selected.close);
 shooter.airRangeBonus=99;check(!combatquery::choose(shooter,victim).close);
 shooter.flags=FixedWeapon;check(combatquery::choose(shooter,victim).slot==0);
 shooter.flags=StagedWeapon;check(combatquery::choose(shooter,victim).slot==7);
 victim.flags=0;check(combatquery::choose(shooter,victim).slot==0);
 shooter.flags=0;victim.armor=1;q.selectMask=2;check(combatquery::choose(shooter,victim).slot==7);
 victim.armor=11;check(combatquery::choose(shooter,victim).index==-1);victim.armor=0;
 shooter.flags=FixedWeapon;w.slot=18;check(combatquery::choose(shooter,victim).index==-1);w.slot=4;
 w.range=65536;check(combatquery::choose(shooter,victim).index==-1);w.range=500;
 shooter.x=INT_MIN;victim.x=INT_MAX;check(combatquery::choose(shooter,victim).index==-1);shooter.x=0;
 victim.x=0;victim.flags=Airborne;shooter.airRangeBonus=65536;
 check(combatquery::choose(shooter,victim).index==-1);
 victim.x=0;victim.y=100;victim.flags=0;shooter.airRangeBonus=0;

 uint32_t weapon=0;int slot=-1,close=-1,air=-1;
 check(options.nativeCombatQueries==0);
 victim.x=INT_MAX;check(combatWeaponQuery(Shooter,Victim,&weapon,&slot,&close,&air)==0);victim.x=0;
 victim.flags=Airborne;shooter.airRangeBonus=-500;
 check(combatWeaponQuery(Shooter,Victim,&weapon,&slot,&close,&air)==0);victim.flags=0;shooter.airRangeBonus=0;
 check(combatWeaponQuery(Shooter,Victim,&weapon,&slot,&close,&air)==1);
 check(weapon==WeaponPointer&&slot==4&&close==1&&air==0);
 check(nativeWeaponCalls==0&&nativeAirCalls==0);
 check(objectAirQuery(Victim,&air)==1&&air==0&&nativeAirCalls==0);
 options.nativeCombatQueries=1;
 check(combatWeaponQuery(Shooter,Victim,&weapon,&slot,&close,&air)==1);
 check(weapon==WeaponPointer+1&&slot==17&&close==0&&air==1&&nativeWeaponCalls==1);
 check(objectAirQuery(Victim,&air)==1&&nativeAirCalls==1);
 check(combatWeaponQuery(Shooter,Victim,nullptr,&slot,&close,&air)==0&&nativeWeaponCalls==1);
 check(objectAirQuery(0,&air)==0&&nativeAirCalls==1);
 check(combatWeaponQuery(Shooter,0x400000,&weapon,&slot,&close,&air)==0&&nativeWeaponCalls==1);
 check(objectAirQuery(0x400000,&air)==0&&nativeAirCalls==1);
 options.nativeCombatQueries=0;
 check(combatWeaponQuery(Shooter,Victim,&weapon,&slot,&close,&air)==1&&nativeWeaponCalls==1);
 check(objectAirQuery(Victim,&air)==1&&air==0&&nativeAirCalls==1);
 check(combatWeaponQuery(Shooter,0x400000,&weapon,&slot,&close,&air)==0&&nativeWeaponCalls==1);
 check(objectAirQuery(0x400000,&air)==0&&nativeAirCalls==1);
 std::printf("combat query policy: %d checks passed\n",checks);
}
