#define main existingRoundTransitionChecks
#include "/workspace/scratch/40121bc881b8/ra2-ui-146/source/unit_monitor/tests/round_transition.cpp"
#undef main
int main(){
 Fixture f;constexpr uint32_t Building=0x240000;
 std::memcpy(f.m.data.data()+Building,f.m.data.data()+Infantry,4096);
 std::memcpy(f.m.data.data()+Building+36,"PWR",4);
 f.m.vector(0xA83C68,0x300400,{Building});
 auto sample=[&](uint32_t tick,bool buildings)->const um::Snapshot&{f.m.bookends=0;return f.sampler->sample(tick,buildings);};
 auto initial=sample(100,true).match;assert(sample(101,true).typeCount==2);
 assert(sample(102,false).typeCount==1);
 f.m.fail=Building+1750;
 const auto&bad=sample(103,true);assert(bad.status==0&&bad.typeCount==0&&bad.playerCount==0);
 assert(f.sampler->probe().stage==external::layout::Stage::UnreadableTypeCatalogue);
 f.m.fail=0;const auto&recovered=sample(104,true);assert(recovered.status==1&&recovered.typeCount==2&&recovered.match==initial);
 sample(105,false);f.m.fail=Building+1750;
 const auto&hidden=sample(606,false);assert(hidden.status==1&&hidden.typeCount==1);
 f.m.fail=0;assert(sample(607,true).typeCount==2);
 sample(608,false);f.m.put(0xA8E348+16,4097);const auto&oversized=sample(1108,true);assert(oversized.status==0&&oversized.typeCount==0);
 f.m.put(0xA8E348+16,1);assert(sample(1109,true).typeCount==2);
 std::cout<<"PASS catalogue buildings toggle, failed-toggle retry, diagnostic, stable match, hidden-category absence and oversized-count recovery\n";
 return 0;
}
