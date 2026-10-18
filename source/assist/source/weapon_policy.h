#pragma once
#include "protocol.h"
namespace assist { namespace planner {
inline bool magnetic(const Weapon& w){return (w.flags&(MagneticWeapon|LocomotorWeapon))==(MagneticWeapon|LocomotorWeapon);}
inline bool magnetron(const Actor& a){for(const auto&w:a.weapons)if(magnetic(w)&&w.range>0)return true;return false;}
inline int cooldown(const Actor&a,int frame){return remaining(a.rearmStart,a.rearmDuration,frame);}
inline bool magneticGroundTarget(const Actor& t){return t.kind==2&&!(t.flags&(Airborne|Manipulated|Organic|Protected|Transported|Cloaked|Limbo|OffMap));}
inline int weaponIndex(const Actor&s,const Actor&t){
 if(s.flags&FixedWeapon)return 0;
 if(s.flags&StagedWeapon)return (t.flags&Airborne)?1:0;
 const auto&p=s.weapons[0];const auto&q=s.weapons[1];if(q.range<=0)return 0;
 if((p.flags&LocomotorWeapon)&&t.kind==3)return 1;
 uint32_t bit=t.armor>=0&&t.armor<11?1u<<t.armor:0;
 uint32_t pm=p.selectMask?p.selectMask:p.armorMask,qm=q.selectMask?q.selectMask:q.armorMask;
 if(!(qm&bit))return 0;if(!(pm&bit))return 1;
 if((t.flags&Airborne)&&(q.flags&AirWeapon))return 1;
 return 0;
}
inline bool compatibleTarget(const Actor&s,const Actor&t,bool mag){
 if(t.armor<0||t.armor>10||(t.flags&(Protected|Transported|Cloaked|Limbo|OffMap))||(s.flags&OutOfAmmo))return false;
 const auto&w=s.weapons[weaponIndex(s,t)];
 if(w.range<=0||w.range>=65536||w.minimum<0||w.minimum>w.range||!(w.armorMask&(1u<<t.armor))||(w.flags&HealingWeapon))return false;
 if((w.flags&EffectivenessKnown)&&!(w.effectiveness[t.armor]>0.02f))return false;
 if(mag&&(!magnetic(w)||!magneticGroundTarget(t)))return false;
 if(magnetic(w)&&!magneticGroundTarget(t))return false;
 if((w.flags&MindControlWeapon)&&(t.flags&(PsionicImmune|Controlled)))return false;
 if((w.flags&PsychicWeapon)&&(t.flags&PsychicImmune))return false;
 if((w.flags&ParasiteWeapon)&&(t.kind==3||(t.flags&(ParasiteImmune|Infested|Airborne))))return false;
 if((w.flags&TemporalWeapon)&&(t.flags&TemporalImmune))return false;
 if((t.flags&TemporalTarget)&&!(w.flags&TemporalWeapon))return false;
 return (t.flags&Airborne)?(w.flags&AirWeapon)!=0:(w.flags&GroundWeapon)!=0;
}
inline bool canTarget(const Actor&s,const Actor&t,int64_t distance,bool mag,int acquisitionPadding=0){
 if(distance<0||distance>0xFFFFFFFFll||!compatibleTarget(s,t,mag))return false;const auto&w=s.weapons[weaponIndex(s,t)];
 int64_t dz=(s.flags&Airborne)?0:int64_t(s.z)-t.z;if(dz< -1048576||dz>1048576)return false;distance+=dz*dz;
 int64_t range=int64_t(w.range)+((t.flags&Airborne)?s.airRangeBonus:0)+acquisitionPadding;if(range<=0||range>=65536)return false;
 return distance<=int64_t(range)*range&&distance>=int64_t(w.minimum)*w.minimum;
}
}}
