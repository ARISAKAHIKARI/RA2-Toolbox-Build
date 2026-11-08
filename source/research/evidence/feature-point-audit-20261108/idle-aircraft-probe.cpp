#define main existingEnemyTestMain
#include "/workspace/scratch/40121bc881b8/ra2-audit-followup/source/assist/tests/enemy_markers.cpp"
#undef main
int main(){EnemyFixture f; f.now=1000;
 const auto&source=f.sampler.sample(f.now,assist::Capacity,true,3);
 auto&externalSampler=f.sampler;const auto*externalData=&source;const auto&snap=source;
 struct{bool idleAircraft=true;}extra;bool enabled=true;
 auto foregroundGame=[](){return true;};uint32_t renderNow=0;
 auto sampleRenderClock=[&](void*){return renderNow;};
 assert(source.status==1&&source.renderStatus==1&&source.width>0&&source.height>0&&externalSampler.idleAircraftTick==1000);
 std::cout<<"{\"samples\":[";bool comma=false;
 for(uint32_t tick:{1000u,1500u,1501u,2000u}){renderNow=tick;externalSampler.refreshCamera(renderNow);
 bool hidden=(!extra.idleAircraft||!enabled||!externalData||snap.renderStatus!=1||!foregroundGame()||snap.chat||
    !externalSampler.idleAircraftTick||uint32_t(sampleRenderClock(nullptr)-externalSampler.idleAircraftTick)>500||snap.width<=0||snap.height<=0);if(comma)std::cout<<',';comma=true;
 std::cout<<"{\"render_tick\":"<<renderNow<<",\"sample_frame\":"<<source.frame<<",\"idle_sample_tick\":"<<externalSampler.idleAircraftTick<<",\"button_hidden\":"<<hidden<<'}';}
 std::cout<<"]}\n";}
