#pragma once
#include "protocol.h"
#include "additional_settings.h"
namespace assist {
// Connection demand is independent of the optional external display sample.
// These bits are consumed by Runtime's input, maintenance or order paths.
// Capacity/Iron/Production/etc. are external presentation and are excluded.
constexpr uint32_t RuntimeFeatureFlags=AutoRepair|AutoCS|NativeOrders|EjectAssist|
 UnsafeTech|AdvancedInput|SmartUnloadOrders|AutoOreReturn|BuildAnywhere|CustomCS|
 MagnetronInput|SpecialClickFlags|ExperimentalReveal|RepairUnrepairable|
 ReserveConstruction|AirWaypoints|AntiTheft|NativeAirWaypoints;
inline bool runtimeFeatureDemand(uint32_t configured,const AdditionalSettings&extra,
                                 bool techMode,bool buildTimeTooltip){
 if((configured&RuntimeFeatureFlags)||techMode||buildTimeTooltip)return true;
 // Use the same effective dependency policy as syncAdditional(): a saved
 // command-owner preference alone is not an enabled native-control feature.
 return extra.smallMapClamp||extra.controlRequest()!=controlmode::Off||extra.commandOwnerRequested()||
  extra.networkHouseRequested()||extra.renderReveal||extra.crcMapShadow||
  extra.nativeSellCursor||extra.nativeMegaGateway||extra.techEnhance||
  extra.sidebarEnabled||extra.viewZoomEnabled||extra.lowDamageFilter||extra.formationEnabled||(extra.moFeatures&23u)||extra.dodgeEnabled||
  extra.missileDefense||extra.magnetronGlobal||extra.magnetronDrop;
}
}
