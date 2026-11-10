#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../source/runtime_feature_demand.h"
#include <cassert>
#include <cstdio>
using namespace assist;
static unsigned checks=0,failures=0;
static void check(bool value){++checks;if(!value)++failures;}
static bool demand(uint32_t flags,const AdditionalSettings&extra,bool displayReady=false,
                   bool tech=false,bool tooltip=false,bool queued=false,bool selection=false,bool flash=false,bool retiringMiner=false){
 // The Host keeps all existing non-feature reasons. Cold fixtures deliberately
 // have no successful external sample, queued command or selection to hide a
 // missing feature demand behind one of those reasons.
 const uint32_t configured=flags;const bool techMode=tech,buildTimeTooltipEnabled=tooltip,hasSelection=selection;
 struct External {int status=1;} external;const auto*externalData=displayReady?&external:nullptr;
 struct Queue {unsigned size;} commandQueue{unsigned(queued)};
 struct Sampler {unsigned flashCount;} externalSampler{unsigned(flash)};
 struct Runtime {unsigned minerDefenseActive;} runtime{unsigned(retiringMiner)};
 const auto*shared=retiringMiner?&runtime:nullptr;
#ifdef RA2_RUNTIME_DEMAND_CALLSITE
#include RA2_RUNTIME_DEMAND_CALLSITE
#else
 bool needsBridge=runtimeFeatureDemand(configured,extra,techMode,buildTimeTooltipEnabled)||(shared&&shared->minerDefenseActive)||(externalData&&externalData->status==1)||commandQueue.size||hasSelection||externalSampler.flashCount;
#endif
 return needsBridge;
}
int main(){
 AdditionalSettings off;
 check(demand(0,off)); // The default attack-own-ground hotkey is a real consumer.
 off.missionModeKeys[selectedmode::OwnGround]=0; // Isolate every cold opt-in below.
 check(!demand(0,off));check(!off.minerDefenseEnabled&&off.minerDefenseRangeCells==1);
 for(uint32_t bit:{1u,2u,3u}){auto settings=off;settings.singlePlayerLagFix=bit;check(demand(0,settings));settings.singlePlayerLagFix=0;check(!demand(0,settings));}
 {auto settings=off;settings.singlePlayerFPS=45;check(demand(0,settings));settings.singlePlayerFPS=0;check(!demand(0,settings));}
 for(uint32_t feature:{1u,2u,4u,7u,16u,23u}){auto settings=off;settings.moFeatures=feature;check(demand(0,settings));settings.moFeatures=0;check(!demand(0,settings));}
 {auto settings=off;settings.superweaponPanel=true;check(!demand(0,settings));}
 const uint32_t bits[]={AutoRepair,AutoCS,NativeOrders,EjectAssist,UnsafeTech,AdvancedInput,
  SmartUnloadOrders,AutoOreReturn,BuildAnywhere,CustomCS,MagnetronInput,AdvancedEnter,
  AircraftSpread,DiskPrimary,SellDrained,SellVehicles,SellEnemy,ExperimentalReveal,
  RepairUnrepairable,ReserveConstruction,AirWaypoints,AntiTheft,NativeAirWaypoints};
 for(auto flag:bits){check(!demand(0,off));check(demand(flag,off));check(!demand(0,off));}
 bool AdditionalSettings::*features[]={&AdditionalSettings::controlAll,&AdditionalSettings::networkHousePreserve,
  &AdditionalSettings::crcMapShadow,&AdditionalSettings::nativeSellCursor,&AdditionalSettings::nativeMegaGateway,
  &AdditionalSettings::superweaponGrant,&AdditionalSettings::techEnhance,&AdditionalSettings::lowDamageFilter,&AdditionalSettings::formationEnabled,
  &AdditionalSettings::keepInputUnlocked,&AdditionalSettings::bombBoardEnabled,&AdditionalSettings::dodgeEnabled,&AdditionalSettings::minerDefenseEnabled,&AdditionalSettings::missileDefense,&AdditionalSettings::magnetronGlobal,
  &AdditionalSettings::magnetronDrop,&AdditionalSettings::viewZoomEnabled,&AdditionalSettings::sidebarEnabled,&AdditionalSettings::smallMapClamp};
 for(auto field:features){auto settings=off;check(!demand(0,settings));settings.*field=true;check(demand(0,settings));settings.*field=false;check(!demand(0,settings));}
 for(unsigned mode=1;mode<=5;++mode){auto settings=off;settings.renderReveal=mode;check(demand(0,settings));}
 check(demand(0,off,false,true));check(demand(0,off,false,false,true));
 check(demand(0,off,true));check(demand(0,off,false,false,false,true));
 check(demand(0,off,false,false,false,false,true));check(demand(0,off,false,false,false,false,false,true));
 auto settings=off;settings.controlCommandOwner=true;check(!demand(0,settings));
 settings.controlAll=true;settings.controlImplementation=1;check(demand(0,settings)&&settings.commandOwnerRequested());
 settings.controlImplementation=0;settings.networkHousePreserve=true;check(demand(0,settings)&&!settings.commandOwnerRequested()&&!settings.networkHouseRequested());
 // Display-only settings and dormant modifiers must not turn a read-only
 // renderer into a Runtime connection request during a failed sample.
 constexpr uint32_t display=Capacity|Iron|Production|AllTransports|Psychic|Crates|TextPanel|CrateResults|Frozen;
 settings=off;settings.enemyMarkers=settings.idleAircraft=settings.parasiteMarker=settings.crateBuffMarkers=true;
 settings.watchEnabled=settings.watchSecondary=settings.watchDanger=settings.passengerBubbles=settings.dangerPassengers=true;
 settings.productionReminderMask=63;settings.nativeCombatQueries=true;settings.nativeAirNetwork=2;
 settings.controlCommandOwner=true;settings.lowDamageNetwork=true;settings.missileReserve=true;
 settings.minerDefenseRangeCells=30;settings.viewZoomPercent=150;settings.sidebarMode=2;settings.burstEnabled=settings.burstHoldEnabled=settings.c4Test=true;settings.scrollRefreshMode=3;
 check(!demand(display,settings));
 // Turning defense off keeps its command owner alive until jobs retire, even
 // with no display sample, no native-input option and no other feature on.
 check(demand(0,off,false,false,false,false,false,false,true));
 check(!demand(0,off,false,false,false,false,false,false,false));
 std::printf("%s %u cold feature 0->1, flag/extra, dependency, existing wake and display-only exclusion checks; failures=%u\n",failures?"FAIL":"PASS",checks,failures);
 return failures?1:0;
}
