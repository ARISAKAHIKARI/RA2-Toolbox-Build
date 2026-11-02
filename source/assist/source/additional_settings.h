#pragma once
#include "player_test_policy.h"
#include "mo_orders.h"
#include "cooperative_types.h"
#include "pointer_intent.h"
namespace assist {
struct AdditionalSettings {
 uint32_t stickyKey='S'|(6<<8);
 uint32_t missionModeKeys[3]{}; // Sleep, Guard, Harmless; empty by default
 bool formationEnabled=false;uint32_t formationHold=18,formationKeys[6]={'Z','X','C','V','B','N'},formationMouse=2;
 bool controlAll=false,switchPlayerEnabled=false;
 bool controlCommandOwner=false;
 bool networkHousePreserve=false,nativeSellCursor=false;
 bool crcMapShadow=false;
 bool nativeMegaGateway=true;
 bool techEnhance=false;int placementImplementation=0;
 bool viewZoomEnabled=false;int viewZoomPercent=100;
 uint32_t moFeatures=0,sidebarMode=0;bool superweaponPanel=false;
 moorders::GesturePolicy moGestures;
 uint32_t viewZoomKeys[3]={0xBD|(6<<8),0xBB|(6<<8),0x08|(6<<8)},sidebarKey=0x7A,pauseKeys[2]={0x74,0x75};
 int scrollRefreshMode=0; // cursor / scrolling / scene / forced redraw
#ifndef RA2_UNIFIED
 int scrollRefreshHz=120; // standalone assist only
#endif
 bool c4Test=false;uint32_t c4Key='C'|(4<<8),c4Mouse=1,c4Mode=0;
 uint32_t renderReveal=0;int mapStrategy=0;bool experimentalReveal=false,antiTheft=false,nativeAirWaypoints=false;
 bool enemyMarkers=false,idleAircraft=false,constructionQueue=false,airWaypoints=false;
 uint32_t productionReminderMask=0; // buildings, defenses, infantry, vehicles, ships, aircraft
 uint32_t airPathKey=0;
 uint32_t nativeAirNetwork=0; // 0=compatibility, 1=UI gate experiment, 2=full native experiment
 bool parasiteMarker=false,crateBuffMarkers=false;
 bool watchEnabled=false,watchSecondary=false,watchDanger=false;int watchSecondaryX=10000,watchSecondaryY=0;uint32_t watchKeys[3]={'W'|(6<<8),'W'|(6<<8),'W'|(6<<8)};
 bool passengerBubbles=false,dangerPassengers=false,magnetronDrop=false;
 bool dodgeEnabled=false;int dodgeMode=0;
 bool burstEnabled=false,burstHoldEnabled=false;uint32_t burstKey=0; // Reserved for configuration compatibility.
 bool missileDefense=false;int missileScanHz=10,missileScope=0,missileAllocation=0;bool magnetronGlobal=false;
 bool missileReserve=true,lowDamageFilter=false,lowDamageNetwork=false;
 bool allHotkeys=false,overrideNative=false;
 int loadMode=0,loadRadius=30;
 uint32_t mouseButtons[pointerinput::MouseCount]={1,1,1,1,1,1,1,2,3,3}; // 0=off, 1=middle, 2/3=side, 4=right
 bool enter=false,air=false,disk=false,sellDrained=false,sellVehicles=false,sellEnemy=false,frozen=false;
 int aircraftAllocationPolicy=0; // 0=quotas, 1=5-cell vehicle coverage, 2=single target + deselect sent
 uint32_t diskKey=17,sellKey=17;
 uint32_t enterKey=18,airKey1=17,airKey2=0,variantKey='T'|(2<<8);
 cooperative::Quota defaults{};
};
enum ExtraControl {LoadLogic,LoadRadius,EnterEnabled,EnterPreset,EnterCustom,AirEnabled,AirPreset1,AirCustom1,AirPreset2,AirCustom2,DefaultFirst,DefaultSecond,DefaultMaximum,QuotaType,QuotaID,QuotaOverride,QuotaFirst,QuotaSecond,QuotaMaximum,DiskEnabled,SellDrainedEnabled,SellVehiclesEnabled,SellEnemyEnabled,FrozenEnabled,VariantKey,DiskPreset,DiskCustom,SellPreset,SellCustom,MouseFirst,RevealExperimental=MouseFirst+pointerinput::MouseCount,EnemyMarkersEnabled,AircraftPolicy,ExtraControlCount};
inline int holdPreset(uint32_t key){return !key?0:key==18?1:key==17?2:key==16?3:4;}
inline uint32_t presetKey(int preset,uint32_t custom){return preset==0?0:preset==1?18:preset==2?17:preset==3?16:custom;}
}



