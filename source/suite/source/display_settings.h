#pragma once
#include <array>
namespace suite {
enum DisplayOption {RefreshMode,RefreshHz,EnemyMarkers,Iron,Capacity,AllTransports,Frozen,Parasite,CrateBuffs,LabelScale,InfoFPS,DisplayOptionCount};
enum class DisplayControl {Check,Choice,Number};
struct DisplaySpec {int id,min,max;DisplayControl control;};
constexpr DisplaySpec displaySpecs[]={
 {886,0,2,DisplayControl::Choice},{887,60,240,DisplayControl::Number},
 {785,0,1,DisplayControl::Check},{301,0,1,DisplayControl::Check},
 {300,0,1,DisplayControl::Check},{303,0,1,DisplayControl::Check},
 {723,0,1,DisplayControl::Check},{892,0,1,DisplayControl::Check},
 {893,0,1,DisplayControl::Check},{816,50,250,DisplayControl::Number},
 {310,5,500,DisplayControl::Number}
};
inline bool validDisplayEdit(int option,int value){return option>=0&&option<DisplayOptionCount&&value>=displaySpecs[option].min&&value<=displaySpecs[option].max;}
struct DisplayState {
 std::array<int,DisplayOptionCount> values{};
 int refreshStatus=0,rateMilli=0,gapUs=0;bool assistEnabled=false;
 bool operator==(const DisplayState&o)const{return values==o.values&&refreshStatus==o.refreshStatus&&rateMilli==o.rateMilli&&gapUs==o.gapUs&&assistEnabled==o.assistEnabled;}
};
}
