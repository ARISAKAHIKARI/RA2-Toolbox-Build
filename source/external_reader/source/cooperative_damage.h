#pragma once
#include "game_view.h"
#include <limits>
namespace external { namespace cooperativedamage {
struct Estimate {double damage=0;uint32_t weapon=0;int slot=-1,shots=0;};
// Reuse the supported selector policy (primary/secondary, IFV, staged and elite
// slots). The estimate is one ordinary firing volley, NOT all future sorties,
// a hit guarantee, or an implementation of arbitrary modded weapon effects.
template<class Read>Estimate estimate(const GameView<Read>&game,uint32_t source,uint32_t target,
                                    const assist::planner::Actor&s,const assist::planner::Actor&t){
 using namespace assist::planner;Estimate out;
 if(!compatibleTarget(s,t,false)||s.flags&(Disabled|Manipulated|Transported|OutOfAmmo|TemporalTarget))return out;
 int selected=weaponIndex(s,t);const auto&info=s.weapons[selected];int slot=info.slot;
 if(slot<0||slot>=18||info.flags&(HealingWeapon|MindControlWeapon|TemporalWeapon|ParasiteWeapon|LocomotorWeapon|MagneticWeapon))return out;
 uint32_t weapon=game.template get<uint32_t>(s.type+0x898+slot*28);
 if(game.template get<float>(source+0x150)>=2){uint32_t elite=game.template get<uint32_t>(s.type+0xA94+slot*28);if(elite)weapon=elite;}
 if(weapon<0x10000)return out;
 uint32_t warhead=game.template get<uint32_t>(weapon+0xAC);if(warhead<0x10000||t.armor<0||t.armor>10)return out;
 const double invalid=std::numeric_limits<double>::quiet_NaN();
 double verses=game.template get<double>(warhead+0xA0+t.armor*8,invalid);
 int base=game.template get<int>(weapon+0xA4),burst=game.template get<int>(weapon+0x9C),index=game.template get<int>(source+0x3B8);
 if(base<=0||!std::isfinite(verses)||verses<=0||verses>=1000000||burst<1||burst>256||index<0||index>burst||(index>0&&index<burst))return out;
 int shots=burst;
 if(game.template get<int>(s.type+0x684,-1)>=0){int ammo=game.template get<int>(source+0x2FC);if(ammo<=0||ammo>1000000)return out;shots=std::min(shots,ammo);}
 // Existing crate/status readers verify these Foot double fields. Reject an
 // unavailable/nonpositive modifier instead of fabricating effective damage.
 double firepower=game.template get<double>(source+0x160,invalid),armor=t.kind==3?1.0:game.template get<double>(target+0x158,invalid);
 if(!std::isfinite(firepower)||!std::isfinite(armor)||firepower<=0||firepower>1024||armor<=0||armor>1024)return out;
 double damage=double(base)*verses*firepower/armor*shots;
 if(!std::isfinite(damage)||damage<=0||damage>assist::cooperative::MaximumExpectedDamage)return out;
 out.damage=damage;out.weapon=weapon;out.slot=slot;out.shots=shots;return out;
}
}}
