// Production command Graph -> off-thread identity index projection, using
// controlled game pages. No native EventClass or transport is mocked as safe.
#define WORLD_FRAME_FIXTURE_ONLY
#include "world_frame_core.cpp"
#include "../../shared/source/world_frame_command_index.h"
#include <cassert>
#include <cstdio>
static bool sourceReady=true;
static bool valid(){return sourceReady;}
int main(){
 using namespace fixture;
 Memory memory;setup(memory);
 Read read{&memory};
 worldframe::Snapshot sample;sample.reset(key());
 worldframe::CaptureScratch scratch;
 worldframe::CaptureGraph<Read>capture(sample,read,scratch);
 CHECK(capture.capture(worldframe::Base,{})&&sample.seal());
 worldframe::CommandRoster worker;
 std::vector<uint32_t>rosterScratch;
 CHECK(worker.build(sample,true,rosterScratch));
 CHECK(worker.current(sample.key)&&worker.count==3&&worker.items==TechnoList);
 CHECK(worker.objects->contains(ActorA)&&worker.objects->contains(ActorB));
 CHECK(!worker.objects->contains(Passenger)); // not in Techno roster
 CHECK(worker.objects->byId(ActorA+5)==ActorA);
 CHECK(worker.objects->byId(ActorB+5)==ActorB);
 CHECK(worker.members.size()==3&&worker.members[0].pointer==ActorA&&worker.members[0].id==ActorA+5);
 CHECK(worker.members[1].pointer==ActorB&&worker.members[1].id==ActorB+5);
 CHECK(worker.globalVehicles.size()==3); // exact duplicated Techno roster
 const auto*workerHash=worker.objects.get();
 worldframe::CommandRoster main;
 main.swap(worker);
 CHECK(main.objects.get()==workerHash&&!worker.objects&&main.current(sample.key));
 CHECK(main.members.size()==3&&main.members[2].pointer==ActorA);
 assist::ObjectIndexView view;
 view.add(0x70000,77);CHECK(view.byId(77)==0x70000);
 view.bind(&main.objects,valid);
 CHECK(view.byId(ActorA+5)==ActorA&&!view.byId(77));
 sourceReady=false;
 CHECK(!view.contains(ActorA)&&!view.byId(ActorB+5));
 sourceReady=true;
 CHECK(view.byId(ActorB+5)==ActorB);
 // Handoff the next worker generation using an O(1) index owner swap.
 worldframe::CommandRoster changed;
 CHECK(changed.build(sample,false,rosterScratch));
 const auto*secondHash=changed.objects.get();
 main.swap(changed);
 CHECK(main.objects.get()==secondHash&&changed.objects.get()==workerHash);
 CHECK(!main.global&&main.globalVehicles.empty()&&view.byId(ActorA+5)==ActorA);
 CHECK(main.members.size()==3&&main.members[1].id==ActorB+5);
 view.unbind();CHECK(view.byId(77)==0x70000&&!view.byId(ActorA+5));
 worldframe::Snapshot missing;missing.reset(key());
 memory.denied.push_back({ActorA+0x10,4});
 worldframe::CaptureGraph<Read>missingGraph(missing,read,scratch);
 CHECK(missingGraph.capture(worldframe::Base,{})&&missing.seal());
 worldframe::CommandRoster rejected;
 CHECK(!rejected.build(missing,false,rosterScratch));
 CHECK(!rejected.current(missing.key)&&!rejected.objects->byId(ActorB+5));
 memory.denied.clear();
 // A sealed Base-only graph never blesses an arbitrary mutated native frame.
 auto changedKey=sample.key;++changedKey.frame;
 CHECK(!main.current(changedKey));
 std::printf("PASS worker command index: %zu checks, exact Techno membership, global vehicles, missing UID fail-close, O(1) owner transfer and stale-view rejection\n",checks);
}
