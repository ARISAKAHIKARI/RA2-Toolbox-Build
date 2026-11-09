// Actual TacticalSampler over immutable production Snapshot pages. UI-only
// reads are a separate seam, so a paused aircraft refresh cannot use them.
#define main existingEnemyTests
#include "enemy_markers.cpp"
#undef main
#include "../../shared/source/world_frame.h"
struct AircraftReadState {
 EnemyMemory*live=nullptr;worldframe::Token source;bool complete=true;size_t reads=0,liveReads=0;
};
struct AircraftRead {
 AircraftReadState*s;
 bool operator()(uint32_t at,void*out,size_t n)const{++s->reads;const bool ok=s->source&&s->source->read(at,out,n);s->complete&=ok;return ok;}
 bool fresh(uint32_t at,void*out,size_t n)const{return operator()(at,out,n);}
 bool live(uint32_t at,void*out,size_t n)const{++s->liveReads;return s->live->read(at,out,n,true);}
};
static worldframe::Token seal(EnemyFixture&f,uint32_t serial,uint32_t tick,uint32_t missing=0){
 auto out=std::make_shared<worldframe::Snapshot>();worldframe::Key key;key.pid=1;key.owner=2;key.match=3;key.serial=serial;key.frame=f.frame;key.tick=tick;key.house=EnemyFixture::local;
 out->reset(key);auto read=[&](uint32_t at,void*p,size_t n){return f.mem.read(at,p,n);};
 for(uint32_t at=0x10000;at<f.mem.data.size();at+=worldframe::PageSize)if(at!=(missing&~4095u))assert(out->prefetch(read,at,worldframe::PageSize));
 assert(out->seal());return out;
}
int main(){
 using F=EnemyFixture;auto fixture=std::make_unique<F>();auto&f=*fixture;
 f.mem.put(F::aircraft+0x21C,F::local);f.mem.put(F::building+0x21C,F::local);f.mem.vector(0xA8E390,0x880000,{F::aircraft});
 f.mem.put(F::airType+0x3EC,uint32_t(0x890000));f.mem.put(F::airType+0x3F0,1);f.mem.put(F::airType+0x3F8,1);f.mem.put(0x890000,F::buildingType);
 f.mem.put(F::aircraft+0x5A4,F::building);f.mem.put(F::aircraft+0xAC,7);f.mem.put(F::aircraft+0xB4,-1);f.mem.put(F::aircraft+0x2FC,4);f.mem.put(F::airType+0x684,4);
 AircraftReadState state{&f.mem,seal(f,10,1000)};external::TacticalSampler<AircraftRead>sampler(AircraftRead{&state});sampler.worldEpoch(3);
 auto stable=sampler.sample(1000,assist::Capacity,true,3,10);assert(state.complete&&sampler.idleAircraftCount==1&&sampler.idleAircraftIDs[0]==7);sampler.commitSample();
 auto shown=[&](uint32_t now,bool confirmed){return sampler.idleAircraftDisplay().current(3,state.source->key.frame,state.source->key.serial,now,confirmed);};
 for(uint32_t now:{1000u,1500u,1501u,60000u}){sampler.refreshCamera(now);assert(shown(now,true));assert(sampler.idleAircraftTick==1000);}
 assert(!shown(1501,false)&&shown(1500,false));
 // Pause in a publication newer than the throttled aircraft sub-sample.
 f.next();state.source=seal(f,11,1050);state.complete=true;stable=sampler.sample(1050,assist::Capacity,true,3,11);sampler.commitSample();
 assert(state.complete&&sampler.idleAircraftDisplay().serial==10&&!shown(1551,true));
 f.mem.vector(0xA8E390,0x880000,{});state.liveReads=state.reads=0;
 assert(sampler.refreshAircraft(1050,11)&&state.complete&&state.reads>0&&state.liveReads==0);
 assert(sampler.idleAircraftDisplay().serial==10);sampler.commitSample();
 assert(sampler.idleAircraftDisplay().serial==11&&sampler.idleAircraftCount==1&&sampler.idleAircraftIDs[0]==7&&sampler.idleAircraftTick==1050&&shown(60000,true));
 // The live array was already empty, but the derived read above used only the
 // pinned bytes. A failure must restore the last committed command cohort.
 f.mem.vector(0xA8E390,0x880000,{F::aircraft});f.next();state.source=seal(f,12,1200,0x880000);state.complete=true;
 sampler.sample(1200,assist::Capacity,true,3,12);assert(!state.complete&&sampler.idleAircraftDisplay().serial==11);sampler.invalidateSample(&stable);
 assert(sampler.idleAircraftCount==1&&sampler.idleAircraftIDs[0]==7&&sampler.idleAircraftTick==1050&&sampler.idleAircraftDisplay().serial==11);
 state.complete=true;assert(!sampler.refreshAircraft(1200,12)&&!state.complete);sampler.invalidateSample(&stable);assert(sampler.idleAircraftCount==1&&sampler.idleAircraftIDs[0]==7);
 f.mem.put(F::aircraft+0x10,uint32_t(9));f.next();state.source=seal(f,13,1300);state.complete=true;stable=sampler.sample(1300,assist::Capacity,true,3,13);sampler.commitSample();
 assert(state.complete&&sampler.idleAircraftCount==1&&sampler.idleAircraftIDs[0]==9&&sampler.idleAircraftDisplay().serial==13);
 // A valid empty list removes stale aircraft and remains a valid paused UI.
 f.mem.vector(0xA8E390,0x880000,{});f.next();state.source=seal(f,14,1350);state.complete=true;stable=sampler.sample(1350,assist::Capacity,true,3,14);sampler.commitSample();
 assert(sampler.idleAircraftCount==1&&!shown(1900,true));state.liveReads=0;assert(sampler.refreshAircraft(1350,14)&&state.complete&&state.liveReads==0);sampler.commitSample();
 assert(sampler.idleAircraftCount==0&&sampler.aircraftTotal==0&&shown(60000,true)&&sampler.idleAircraftTick==1350);
 const auto sample=sampler.idleAircraftDisplay();assert(!sample.current(4,stable.frame,14,1350,true));assert(!sample.current(3,stable.frame-1,14,60000,true));assert(!sample.current(3,stable.frame,13,60000,true));
 auto wrapped=sample;wrapped.tick=0xfffffff0u;assert(wrapped.current(3,wrapped.frame,wrapped.serial,484,false));assert(!wrapped.current(3,wrapped.frame,wrapped.serial,485,false));
 // A missing field on the last aircraft cannot become readable within this
 // sealed source. Repeated paused Host polls must not rescan all 8192 entries.
 const uint32_t tail=0x900C00;std::memcpy(f.mem.data.data()+tail,f.mem.data.data()+F::aircraft,0x800);
 std::vector<uint32_t>many(8192,F::aircraft);many.back()=tail;f.mem.vector(0xA8E390,0x910000,many);
 f.next();state.source=seal(f,15,1375,tail+0x6CA);state.complete=true;
 stable=sampler.sample(1375,0,false,0,15);assert(state.complete);sampler.commitSample();
 assert(sampler.idleAircraftDisplay().serial==14&&!shown(60000,true));
 state.liveReads=state.reads=0;assert(!sampler.refreshAircraft(1375,15)&&!state.complete);const auto failedReads=state.reads;assert(failedReads>8192&&state.liveReads==0);
 sampler.invalidateSample(&stable);assert(sampler.idleAircraftDisplay().serial==14&&sampler.idleAircraftTick==1350);
 state.complete=true;state.reads=0;assert(!sampler.refreshAircraft(1375,15));const auto repeatedReads=state.reads;
 std::cout<<"sealed_tail_failure entries=8192 first_reads="<<failedReads<<" repeated_reads="<<repeatedReads<<std::endl;
 assert(repeatedReads==0&&state.complete&&state.liveReads==0);sampler.invalidateSample(&stable);
 // A full tactical sample (for a changed UI/configuration) clears the failed
 // derived attempt; the same sealed source may then be tried once again.
 state.complete=true;sampler.sample(1375,assist::Capacity,false,0,15);sampler.invalidateSample(&stable);
 state.complete=true;state.reads=0;assert(!sampler.refreshAircraft(1375,15)&&state.reads==failedReads&&!state.complete);sampler.invalidateSample(&stable);
 // The next real publication can recover without changing the display tick
 // on any of the rejected attempts above.
 f.mem.vector(0xA8E390,0x880000,{F::aircraft});f.next();state.source=seal(f,16,1500);state.complete=true;
 stable=sampler.sample(1500,assist::Capacity,false,0,16);assert(state.complete);sampler.commitSample();
 assert(sampler.idleAircraftDisplay().serial==16&&sampler.idleAircraftTick==1500&&sampler.idleAircraftCount==1&&sampler.idleAircraftIDs[0]==9);
 sampler.worldEpoch(4);assert(!sampler.idleAircraftDisplay().tick&&sampler.idleAircraftCount==0);sampler.reset();assert(!sampler.idleAircraftDisplay().tick);
 std::cout<<"PASS paused same-source aircraft display; lagging serial sealed-only refresh; immutable timestamp; lost confirmation; failed-candidate count/ID rollback; sealed failure retry gate; configuration reset; next-frame recovery; valid empty list; epoch/frame/serial isolation and tick wrap\n";
}
