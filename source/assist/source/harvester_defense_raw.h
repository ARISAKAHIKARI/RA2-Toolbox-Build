#pragma once
// Existing Unit/Weapon/Warhead fields only. Do not use the unverified
// ParasiteImUsing/ArchiveTarget candidates or invoke a game decision function.
// Defense has its own participant filter. Automatic-unload type restrictions
// must not change either its default miners or a selected miner's native Dock.
static bool minerDefenseMiner(uint32_t type){
 int capacity=0;uint8_t flags[2]{};
 return type>=0x10000&&read(type+0x800,&capacity,sizeof capacity)&&capacity>0&&
  read(type+0xE0E,flags,sizeof flags)&&(flags[0]||flags[1]);
}
static bool minerDefenseType(uint32_t type,int k){
 if((k!=1&&k!=2)||type<0x10000||!refreshFeatureTypes())return false;
 if(!featureConfig.filters[featuretypes::SpiderDefense].custom)return k==2&&minerDefenseMiner(type);
 return featureTypeAllowed(featuretypes::SpiderDefense,type);
}
static bool minerDefenseDock(uint32_t unit,uint32_t refinery){
 if(kind(unit)!=2||kind(refinery)!=3)return false;const uint32_t type=typeOf(unit,2),dock=typeOf(refinery,3);
 uint8_t disabled=1;if(!minerDefenseMiner(type)||!dock||!read(refinery+0x1C8,&disabled,sizeof disabled)||disabled)return false;
 Vec docks=vec(type+0x3E8,256);for(int n=0;n<docks.count&&n<8;++n)if(item(docks,n)==dock)return true;return false;
}
static bool minerDefenseThreat(uint32_t p,uint32_t house,bool cached=false){
 if(!p||kind(p)!=2||!nativeEnemy(p,house)||get<int>(p+0x6C)<=0||!get<bool>(p+0x74)||
    !get<bool>(p+0x90)||get<bool>(p+0x81)||get<uint32_t>(p+0x11C)||get<int>(p+0x220)||rawObjectAirborne(p,2))return false;
 const uint32_t type=typeOf(p,2);uint8_t naval=1;
 if(!type||!read(type+0xCCE,&naval,sizeof naval)||naval)return false;
 int slots[2]{};const int count=rawWeaponSlots(p,type,slots);const float rank=get<float>(p+0x150);
 for(int n=0;n<count;++n){const uint32_t weapon=rawWeapon(type,rank,slots[n]);
  if(weapon>=0x10000&&(rawWeaponInfo(weapon,cached).flags&planner::ParasiteWeapon))return true;
 }return false;
}
