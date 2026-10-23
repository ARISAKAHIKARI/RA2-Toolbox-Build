#pragma once
#include "player_test_policy.h"
#include "cooperative_types.h"
#include "pointer_intent.h"
namespace assist {
struct AdditionalSettings {
 bool formationEnabled=true;uint32_t formationHold=18,formationKeys[4]={'Z','X','C','V'};
 bool controlAll=false,switchPlayerEnabled=false;
 int scrollRefreshMode=0; // cursor / scrolling / scene / forced redraw
#ifndef RA2_UNIFIED
 int scrollRefreshHz=120; // standalone assist only
#endif
 bool c4Test=false;uint32_t c4Key='C'|(4<<8),c4Mouse=1;
 uint32_t renderReveal=0;int mapStrategy=0;bool experimentalReveal=false,antiTheft=false,nativeAirWaypoints=false;
 bool enemyMarkers=false,idleAircraft=true,constructionQueue=true,airWaypoints=true;
 uint32_t productionReminderMask=63; // buildings, defenses, infantry, vehicles, ships, aircraft
 uint32_t airPathKey=0;
 bool parasiteMarker=true,crateBuffMarkers=true;
 bool watchEnabled=true,watchDanger=true;uint32_t watchKey='W'|(6<<8);
 bool passengerBubbles=true,dangerPassengers=false,magnetronDrop=false;
 bool dodgeEnabled=false;int dodgeMode=0;
 bool burstEnabled=false,burstHoldEnabled=false;uint32_t burstKey=0; // Reserved for configuration compatibility.
 bool missileDefense=false;int missileScanHz=10,missileScope=0,missileAllocation=0;bool magnetronGlobal=false;
 bool allHotkeys=false,overrideNative=false;
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

