#pragma once
#include "cooperative_types.h"
#include "pointer_intent.h"
namespace assist {
struct AdditionalSettings {
 bool experimentalReveal=false;
 bool enemyMarkers=false;
 int loadMode=0,loadRadius=30;
 uint32_t mouseButtons[pointerinput::MouseCount]={1,1,1,1,1,1,1,2,3,3}; // 0=off, 1=middle, 2/3=side, 4=right
 bool enter=false,air=false,disk=false,sellDrained=false,sellVehicles=false,sellEnemy=false,frozen=true;
 uint32_t diskKey=17,sellKey=17;
 uint32_t enterKey=18,airKey1=17,airKey2=0,variantKey='T'|(2<<8);
 cooperative::Quota defaults{};
};
enum ExtraControl {LoadLogic,LoadRadius,EnterEnabled,EnterPreset,EnterCustom,AirEnabled,AirPreset1,AirCustom1,AirPreset2,AirCustom2,DefaultFirst,DefaultSecond,DefaultMaximum,QuotaType,QuotaID,QuotaOverride,QuotaFirst,QuotaSecond,QuotaMaximum,DiskEnabled,SellDrainedEnabled,SellVehiclesEnabled,SellEnemyEnabled,FrozenEnabled,VariantKey,DiskPreset,DiskCustom,SellPreset,SellCustom,MouseFirst,RevealExperimental=MouseFirst+pointerinput::MouseCount,EnemyMarkersEnabled,ExtraControlCount};
inline int holdPreset(uint32_t key){return !key?0:key==18?1:key==17?2:key==16?3:4;}
inline uint32_t presetKey(int preset,uint32_t custom){return preset==0?0:preset==1?18:preset==2?17:preset==3?16:custom;}
}
