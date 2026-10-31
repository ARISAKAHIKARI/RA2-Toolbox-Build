#pragma once
#include <array>
#include <string>
namespace suite {
enum DisplayOption {RefreshMode,EnemyMarkers,Iron,Capacity,AllTransports,Frozen,Parasite,CrateBuffs,LabelScale,InfoFPS,ShroudStrategy,PassengerBubbles,DangerPassengers,InfoAdaptive,RenderUpgrade,RenderProcessing,RenderPhysical,RenderUnified,RenderPalette,RenderPrecision,RenderMaterial,RenderDetail,RenderXbrz,RenderScale,RenderGPU,RenderParticles,RenderLasers,RenderVoxelLighting,RenderSpriteLighting,WatchPrimary,WatchSecondary,WatchX,WatchY,WatchDanger,WatchKey1,WatchKey2,WatchKey3,MapPrimary,MapSecondary,MapMode,MapScale,MapPosition,MapX,MapY,MapAlignX,MapAlignY,MapSecondaryX,MapSecondaryY,MapSecondaryWidth,ProductionHints,PsychicHints,CrateHints,TextHints,IdleAircraft,ReminderBuilding,ReminderDefense,ReminderInfantry,ReminderVehicle,ReminderShip,ReminderAircraft,TextMargin,ToastDuration,RenderHighRefresh,HarvesterCounter,PowerDelta,BuildTimeTooltip,DisplayOptionCount};
enum class DisplayControl {Check,Choice,Number,Hotkey};
struct DisplaySpec {int id,min,max;DisplayControl control;};
constexpr DisplaySpec displaySpecs[]={
 {886,0,0,DisplayControl::Choice},
 {785,0,1,DisplayControl::Check},{301,0,1,DisplayControl::Check},
 {300,0,1,DisplayControl::Check},{303,0,1,DisplayControl::Check},
 {723,0,1,DisplayControl::Check},{892,0,1,DisplayControl::Check},
 {893,0,1,DisplayControl::Check},{816,50,250,DisplayControl::Number},
 {310,5,500,DisplayControl::Number},{808,0,6,DisplayControl::Choice},
 {897,0,1,DisplayControl::Check},{898,0,1,DisplayControl::Check},
 {9160,0,1,DisplayControl::Check},
 {9300,0,1,DisplayControl::Check},
 {9313,0,1,DisplayControl::Check},
 {9306,0,1,DisplayControl::Check},
 {9301,0,1,DisplayControl::Check},
 {9304,0,1,DisplayControl::Choice},
 {9309,0,1,DisplayControl::Choice},
 {9307,0,1,DisplayControl::Check},
 {9308,0,1,DisplayControl::Choice},
 {9314,0,1,DisplayControl::Check},
 {9305,0,3,DisplayControl::Choice},
 {9310,0,1,DisplayControl::Check},
 {9320,0,1,DisplayControl::Check},
 {9321,0,1,DisplayControl::Check},
 {9322,0,1,DisplayControl::Check},
 {9323,0,1,DisplayControl::Check},
 {9060,0,1,DisplayControl::Check},
 {9163,0,1,DisplayControl::Check},
 {9164,0,10000,DisplayControl::Number},
 {9165,0,10000,DisplayControl::Number},
 {9061,0,1,DisplayControl::Check},
 {9062,0,65535,DisplayControl::Hotkey},
 {9063,0,65535,DisplayControl::Hotkey},
 {9064,0,65535,DisplayControl::Hotkey},
 {830,0,1,DisplayControl::Check},
 {9162,0,1,DisplayControl::Check},
 {9065,0,1,DisplayControl::Choice},
 {831,5,200,DisplayControl::Number},
 {850,0,3,DisplayControl::Choice},
 {851,0,32768,DisplayControl::Number},
 {852,0,32768,DisplayControl::Number},
 {9100,0,2,DisplayControl::Choice},
 {9101,0,2,DisplayControl::Choice},
 {9166,0,10000,DisplayControl::Number},
 {9167,0,10000,DisplayControl::Number},
 {9168,160,4096,DisplayControl::Number},
 {302,0,1,DisplayControl::Check},
 {304,0,1,DisplayControl::Check},
 {305,0,1,DisplayControl::Check},
 {306,0,1,DisplayControl::Check},
 {798,0,1,DisplayControl::Check},
 {9070,0,1,DisplayControl::Check},
 {9071,0,1,DisplayControl::Check},
 {9072,0,1,DisplayControl::Check},
 {9073,0,1,DisplayControl::Check},
 {9074,0,1,DisplayControl::Check},
 {9075,0,1,DisplayControl::Check},
 {311,0,4096,DisplayControl::Number},
 {471,500,60000,DisplayControl::Number},
 {9324,0,1,DisplayControl::Check},{9325,0,1,DisplayControl::Check},{9326,0,1,DisplayControl::Check},{9327,0,1,DisplayControl::Check}
};
static_assert(sizeof(displaySpecs)/sizeof(displaySpecs[0])==DisplayOptionCount,"Every shared setting has one owner");
inline bool validDisplayEdit(int option,int value){return option>=0&&option<DisplayOptionCount&&value>=displaySpecs[option].min&&value<=displaySpecs[option].max;}
struct DisplayState {
 std::array<int,DisplayOptionCount> values{};
 int refreshStatus=0,rateMilli=0,gapUs=0,infoRate=500;bool assistEnabled=false;std::wstring renderStatus;
 bool operator==(const DisplayState&o)const{return values==o.values&&refreshStatus==o.refreshStatus&&rateMilli==o.rateMilli&&gapUs==o.gapUs&&infoRate==o.infoRate&&assistEnabled==o.assistEnabled&&renderStatus==o.renderStatus;}
};
}
