#include "../source/settings_dependencies.h"
#include <cassert>
#include <cstdio>
#include <set>
using namespace assist::settingsdeps;
int main(){
 State s;bool saved=true;
 assert(!decision(9313,s).enabled()&&decision(9313,s).missing==Need::Render);
 s.set(Need::Render,true);assert(decision(9313,s).enabled());
 assert(decision(9304,s).missing==Need::Quality&&decision(9314,s).missing==Need::Quality);
 s.set(Need::Quality,true);assert(decision(9304,s).enabled()&&decision(9314,s).enabled()&&decision(9305,s).missing==Need::Reconstruction);
 s.set(Need::Reconstruction,true);assert(effective(saved,9305,s));
 for(auto parent:{Need::Render,Need::Quality,Need::Reconstruction}){
  s.set(parent,false);assert(!effective(saved,9305,s)&&saved);
  s.set(parent,true);assert(effective(saved,9305,s));
 }
 // The GPU master and secondary windows are independent consumers; requiring
 // a primary scene/window would incorrectly disable working configurations.
 State secondary;secondary.set(Need::Watch,true);secondary.set(Need::WatchSecondary,true);
 secondary.set(Need::Minimap,true);secondary.set(Need::MinimapSecondary,true);
 assert(decision(9310,secondary).enabled()&&decision(9162,secondary).enabled()&&decision(9163,secondary).enabled());
 for(int id:{9061,9164,9165,9065,9166,9167,9168})assert(decision(id,secondary).enabled());
 for(int id:{831,850,851,852,9100,9101})assert(!decision(id,secondary).enabled());
 secondary.set(Need::MinimapPrimary,true);assert(decision(831,secondary).enabled()&&decision(850,secondary).enabled());
 assert(decision(851,secondary).missing==Need::CustomMinimap);
 secondary.set(Need::CustomMinimap,true);assert(decision(851,secondary).enabled());
 // Independent danger bubbles/global magnetron automation and action hotkeys
 // must remain configurable without an unrelated display or click-mode parent.
 for(int id:{898,899,9042,200,450,451,9062,780,9496,9497})assert(decision(id,State{}).enabled());
 State filters;filters.set(Need::CustomMiners,true);
 assert(decision(783,filters).missing==Need::MinerAutomation);
 // Miner defense can use the shared miner list with ore return disabled;
 // refinery settings still belong only to the ore-return feature.
 filters.set(Need::MinerAutomation,true);assert(decision(781,filters).enabled()&&decision(783,filters).enabled());
 assert(!decision(782,filters).enabled()&&!decision(784,filters).enabled());
 filters.set(Need::Ore,true);assert(decision(782,filters).enabled()&&!decision(784,filters).enabled());
 filters.set(Need::CustomRefineries,true);assert(decision(784,filters).enabled());
 filters.set(Need::MinerAutomation,false);assert(!decision(783,filters).enabled()&&decision(784,filters).enabled());
 filters.set(Need::LowDamage,true);assert(decision(9183,filters).enabled());
 filters.set(Need::LowDamage,false);assert(!decision(9183,filters).enabled());
 State quota;quota.set(Need::Aircraft,true);quota.set(Need::AircraftQuotas,true);
 assert(decision(714,quota).enabled()&&decision(715,quota).missing==Need::QuotaTarget);
 quota.set(Need::QuotaTarget,true);assert(decision(715,quota).enabled()&&decision(716,quota).missing==Need::QuotaOverride);
 quota.set(Need::QuotaOverride,true);assert(decision(716,quota).enabled());
 quota.set(Need::AircraftQuotas,false);assert(!decision(710,quota).enabled()&&!decision(715,quota).enabled());
 // Each persistent type list follows its own opt-in checkbox. Turning one
 // list on must not unlock any other list, or depend on a live game feature.
 for(int feature=0;feature<assist::featuretypes::Count;++feature){
  if(feature==assist::featuretypes::BurstRefresh)continue;
  State lists;const int id=9201+2*feature;
  assert(!decision(id,lists).enabled()&&decision(id,lists).missing==typeListNeed(feature));
  lists.set(typeListNeed(feature),true);assert(decision(id,lists).enabled());
  for(int other=0;other<assist::featuretypes::Count;++other)if(other!=feature&&other!=assist::featuretypes::BurstRefresh)assert(!decision(9201+2*other,lists).enabled());
  lists.set(typeListNeed(feature),false);assert(!decision(id,lists).enabled());
  assert(decision(id-1,lists).enabled()); // the parent stays editable
 }
 assert(decision(9201+2*assist::featuretypes::BurstRefresh,State{}).enabled());
 // Each unavailable field has a human-readable reason. Product-disabled
 // features stay unavailable even with all settings on; hardware switches
 // are editable until their explicit slot budget says otherwise.
 State all;all.available=~uint64_t(0);State unavailable;unavailable.hardwareBlocked=31;std::set<int> unique;
 for(int id:Controls){
  assert(unique.insert(id).second);const auto ready=decision(id,all);
  assert(ready.enabled()||ready.missing==Need::Unavailable);
  const auto missing=decision(id,unavailable);
  if(id==9096)assert(missing.enabled()); // Selecting the implementation is always allowed.
  else assert(!missing.enabled()&&*reason(missing.missing));
 }
 assert(decision(-1,State{}).enabled());
 std::printf("PASS: settings dependencies (%zu controls), independent consumers and saved-child restoration\n",unique.size());
}
