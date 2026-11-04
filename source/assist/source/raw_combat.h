// Read-only ABI for the supported original YR executable. Verified against
// 0x7177C0/7E0 (weapon arrays), 0x771C70 (weapon layout), 0x6FC94F (rearm),
// and INI readers for Organic / BalloonHover. No virtual/game decision calls.
#include "weapon_policy.h"
static bool rawObjectAirborne(uint32_t p,int k){return k==0||get<bool>(p+0x134)||get<int>(p+0x94)==3||get<bool>(p+0x84)||get<bool>(p+0x8D);}
static uint32_t rawWeapon(uint32_t type,float veterancy,int index){
 uint32_t weapon=get<uint32_t>(type+0x898+index*28);
 if(veterancy>=2.0f&&get<uint32_t>(type+0xA94+index*28))weapon=get<uint32_t>(type+0xA94+index*28);return weapon;
}
static int rawWeaponSlots(uint32_t p,uint32_t type,int(&slots)[2]){
 slots[0]=0;slots[1]=1;
 if(kind(p)==1&&get<bool>(type+0x6AC)){int sequence=get<int>(p+0x6C4);slots[0]=sequence>=27&&sequence<=30?get<int>(type+0x6A8):0;slots[1]=-1;}
 else if(kind(p)==2&&get<bool>(p+0x6E0)&&get<bool>(type+0x6AC)){slots[0]=get<int>(type+0x6A8);slots[1]=-1;}
 else if(get<bool>(type+0xCD5)){int stage=get<int>(p+0x140);slots[0]=stage>=0&&stage<9?stage*2:-1;slots[1]=slots[0]>=0?slots[0]+1:-1;}
 else if(get<int>(type+0x808)>0){int slot=get<int>(p+0x138);slots[0]=slot==-1?0:slot;slots[1]=-1;}
 if(slots[0]<0||slots[0]>=18)return 0;return slots[1]>=0&&slots[1]<18?2:1;
}
static planner::Weapon rawWeaponFresh(uint32_t weapon){
 planner::Weapon info{};if(weapon<0x10000||get<bool>(weapon+0x136))return info;
 info.range=get<int>(weapon+0xB4);info.minimum=get<int>(weapon+0xB8);
 if(info.range<=0||info.range>=65536||info.minimum<0||info.minimum>info.range)return {};
 uint32_t projectile=get<uint32_t>(weapon+0xA0),warhead=get<uint32_t>(weapon+0xAC);if(projectile<0x10000||warhead<0x10000)return {};
 if(get<bool>(projectile+0x2A4))info.flags|=planner::AirWeapon;if(get<bool>(projectile+0x2A5))info.flags|=planner::GroundWeapon;
 if(get<int>(weapon+0xA4)<0)info.flags|=planner::HealingWeapon;
 if(get<bool>(warhead+0x155))info.flags|=planner::MindControlWeapon;if(get<bool>(warhead+0x178))info.flags|=planner::PsychicWeapon;
 if(get<bool>(warhead+0x15A))info.flags|=planner::TemporalWeapon;if(get<bool>(warhead+0x159))info.flags|=planner::ParasiteWeapon;
 if(get<bool>(weapon+0x15C))info.flags|=planner::MagneticWeapon;if(get<bool>(warhead+0x15B))info.flags|=planner::LocomotorWeapon;
 info.flags|=planner::EffectivenessKnown;
 for(int i=0;i<11;++i){double verses=get<double>(warhead+0xA0+i*8);if(verses>0&&verses<1000000)info.effectiveness[i]=float(verses);if(verses>0&&verses<1000000)info.selectMask|=1u<<i;if(verses>0.02&&verses<1000000)info.armorMask|=1u<<i;}
 return info;
}
struct WeaponCacheEntry {uint32_t pointer,match;int frame;planner::Weapon data;};
static WeaponCacheEntry weaponCache[1024];
static planner::Weapon rawWeaponInfo(uint32_t weapon,bool cached){
 if(!cached)return rawWeaponFresh(weapon);int frame=get<int>(0xA8ED84);auto&e=weaponCache[(weapon>>4)&1023];
 if(e.pointer!=weapon||e.match!=match||frame<e.frame||frame-e.frame>=30){e.pointer=weapon;e.match=match;e.frame=frame;e.data=rawWeaponFresh(weapon);}return e.data;
}
static bool rawMagnetron(uint32_t p,featuretypes::Feature feature=featuretypes::Magnetron){
 int k=kind(p);uint32_t type=typeOf(p,k);if(k!=2||!type||!featureTypeAllowed(feature,type))return false;int slots[2],count=rawWeaponSlots(p,type,slots);
 for(int i=0;i<count;++i){uint32_t w=rawWeapon(type,get<float>(p+0x150),slots[i]);if(w>=0x10000&&get<bool>(w+0x15C)){uint32_t wh=get<uint32_t>(w+0xAC);if(wh>=0x10000&&get<bool>(wh+0x15B))return true;}}return false;
}
static void rawCombatInfo(uint32_t p,uint32_t type,int k,planner::Actor& a,bool weapons,bool cached=false){
 a.armor=get<int>(type+0x9C);a.kind=k;a.z=get<int>(p+0xA4);a.target=rawObjectID(get<uint32_t>(p+0x2B4));
 if(get<bool>(type+0xD35))a.flags|=planner::PsionicImmune;if(get<bool>(type+0xD36))a.flags|=planner::PsychicImmune;
 if(!get<bool>(type+0xD3A))a.flags|=planner::TemporalImmune;if(!get<bool>(type+0xD38))a.flags|=planner::ParasiteImmune;
 if(get<uint32_t>(p+0x2C0)&&get<bool>(p+0x2C4))a.flags|=planner::Controlled;
 if(k!=3&&get<uint32_t>(p+0x694))a.flags|=planner::Infested;
 if(get<bool>(p+0x270)||get<uint32_t>(p+0x278))a.flags|=planner::TemporalTarget;
 if(rawObjectAirborne(p,k))a.flags|=planner::Airborne;
 if(get<uint32_t>(p+0x2B0)||get<bool>(p+0x427)||get<uint32_t>(p+0x428)||(k!=3&&get<bool>(p+0x6AD)))a.flags|=planner::Manipulated;
 if(get<bool>(type+0xD97))a.flags|=planner::Organic;
 if(remaining(get<int>(p+0x18C),get<int>(p+0x194),get<int>(0xA8ED84))||get<bool>(type+0x233)||!get<bool>(type+0x231))a.flags|=planner::Protected;
 if(weapons){
  a.rearmStart=get<int>(p+0x2EC);a.rearmDuration=get<int>(p+0x2F4);a.liftTarget=rawObjectID(get<uint32_t>(p+0x2AC));
  int slots[2],count=rawWeaponSlots(p,type,slots);if(count==1)a.flags|=planner::FixedWeapon;else if(get<bool>(type+0xCD5))a.flags|=planner::StagedWeapon;
  a.airRangeBonus=get<int>(type+0x68C);if(a.airRangeBonus< -32768||a.airRangeBonus>32768)a.airRangeBonus=0;
  if(get<int>(type+0x684)>=0&&get<int>(p+0x2FC)<=0)a.flags|=planner::OutOfAmmo;
  uint32_t temporal=get<uint32_t>(p+0x274);if(temporal>=0x10000&&a.target&&rawObjectID(get<uint32_t>(temporal+0x28))==a.target)a.flags|=planner::SustainedFire;
  for(int i=0;i<count;++i){uint32_t ptr=rawWeapon(type,get<float>(p+0x150),slots[i]);a.weapons[i]=rawWeaponInfo(ptr,cached);a.weapons[i].slot=slots[i];
   int range=a.weapons[i].range+a.airRangeBonus;if(range>a.range&&range<65536)a.range=range;
   int burst=get<int>(p+0x3B8);if(ptr>=0x10000&&burst>0&&burst<get<int>(ptr+0x9C))a.flags|=planner::BurstFiring;
  }
 }
}
