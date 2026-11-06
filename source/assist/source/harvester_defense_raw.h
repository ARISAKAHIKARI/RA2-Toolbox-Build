#pragma once
// Existing Unit/Weapon/Warhead fields only. Do not use the unverified
// ParasiteImUsing/ArchiveTarget candidates or invoke a game decision function.
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
