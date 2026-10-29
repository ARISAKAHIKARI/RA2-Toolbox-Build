#include "../source/settings_dependencies.h"
#include <cassert>
#include <cstdio>
#include <set>
using namespace assist::settingsdeps;
int main(){
 State s;bool savedDLSS=true;
 assert(!decision(9311,s).enabled()&&decision(9311,s).missing==Need::Host64);
 s.set(Need::Host64,true);assert(decision(9311,s).missing==Need::Render);
 s.set(Need::Render,true);assert(decision(9311,s).missing==Need::GPU);
 s.set(Need::GPU,true);assert(decision(9311,s).missing==Need::Detail);
 s.set(Need::Detail,true);assert(effective(savedDLSS,9311,s));
 for(auto parent:{Need::Host64,Need::Render,Need::GPU,Need::Detail}){
  s.set(parent,false);assert(!effective(savedDLSS,9311,s)&&savedDLSS);
  s.set(parent,true);assert(effective(savedDLSS,9311,s));
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
 for(int id:{898,899,9042,200,450,451,9062,780})assert(decision(id,State{}).enabled());
 State filters;filters.set(Need::CustomMiners,true);
 assert(decision(783,filters).missing==Need::Ore);
 filters.set(Need::Ore,true);assert(decision(783,filters).enabled()&&!decision(784,filters).enabled());
 filters.set(Need::LowDamage,true);assert(decision(9183,filters).enabled());
 filters.set(Need::LowDamage,false);assert(!decision(9183,filters).enabled());
 State quota;quota.set(Need::Aircraft,true);quota.set(Need::AircraftQuotas,true);
 assert(decision(714,quota).enabled()&&decision(715,quota).missing==Need::QuotaTarget);
 quota.set(Need::QuotaTarget,true);assert(decision(715,quota).enabled()&&decision(716,quota).missing==Need::QuotaOverride);
 quota.set(Need::QuotaOverride,true);assert(decision(716,quota).enabled());
 quota.set(Need::AircraftQuotas,false);assert(!decision(710,quota).enabled()&&!decision(715,quota).enabled());
 // Every constrained field has a human-readable reason and can recover when
 // all its prerequisites are met; no duplicate IDs hide a control update.
 State all;all.available=~uint64_t(0);std::set<int> unique;
 for(int id:Controls){assert(unique.insert(id).second);assert(decision(id,all).enabled());auto missing=decision(id,State{});assert(!missing.enabled()&&*reason(missing.missing));}
 assert(decision(-1,State{}).enabled());
 std::printf("PASS: settings dependencies (%zu controls), independent consumers and saved-child restoration\n",unique.size());
}
