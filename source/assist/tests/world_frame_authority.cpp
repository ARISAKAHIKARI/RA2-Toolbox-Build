#define WORLD_FRAME_PLATFORM_FIXTURE
#define WORLD_FRAME_FIXTURE_ONLY
#include "world_frame_core.cpp"
namespace authority {
using namespace fixture;
void run(){
 currentPid=HostPid;fixture::clock=2000;auto client=worldframe::acquireClient(GamePid,1);client->subscribe(1,worldframe::Base);
 currentPid=GamePid;worldframe::Server control;CHECK(control.wanted(HostPid,fixture::clock));worldframe::Server writer(worldframe::Server::Role::PublicationOnly);CHECK(writer.wanted(HostPid,fixture::clock));
 worldframe::Snapshot frame;fillPages(frame,4,1,1);control.observe(frame.key);CHECK(writer.observes(frame.key));CHECK(writer.publish(frame));currentPid=HostPid;auto first=client->acquire(7);CHECK(first);
 currentPid=GamePid;fillPages(frame,4,2,1);control.observe(frame.key);unsigned boundaries=0;
 // Force retirement at the final cancellation check, just before commit.
 CHECK(writer.publish(frame,[&]{if(++boundaries==3)control.retire();return true;}));CHECK(boundaries==3&&channel()->active>=0&&channel()->observedTick==0);
 currentPid=HostPid;CHECK(!client->acquire(7)&&!client->accepts(first));
 currentPid=GamePid;auto newer=frame.key;++newer.generationLow;control.observe(newer);CHECK(!writer.observes(frame.key));
 currentPid=HostPid;CHECK(!client->acquire(7));
 currentPid=GamePid;frame.key=newer;frame.key.serial=3;CHECK(writer.publish(frame,[&]{return writer.observes(frame.key);}));const auto observationSeq=channel()->observationSeq;
 currentPid=HostPid;auto third=client->acquire(7);CHECK(third&&third->key.serial==3);currentPid=GamePid;
 writer.disconnect();CHECK(channel()->observationSeq==observationSeq&&channel()->observedTick==2000&&channel()->active>=0);
 currentPid=HostPid;CHECK(client->acquire(7)==third);
 // Retirement uses tick zero, including the first 500 ms after boot/wrap.
 fixture::clock=100;channel()->observedTick=0;CHECK(!client->unchanged(third,100)&&!client->accepts(third)&&!client->acquire(7));
 currentPid=GamePid;control.disconnect();currentPid=HostPid;CHECK(!client->acquire(7));
 std::puts("PASS publication authority: independent writer, late commit after retirement rejected while fresh, old generation rejected, worker disconnect preserves game-owned observations and current frame");
}
}
int main(){authority::run();}
