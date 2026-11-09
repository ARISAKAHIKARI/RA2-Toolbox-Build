#define WORLD_FRAME_PLATFORM_FIXTURE
#define WORLD_FRAME_FIXTURE_ONLY
#include "world_frame_core.cpp"
namespace notifications {
using namespace fixture;
bool take(const std::shared_ptr<void>&handle){CHECK(handle);auto&set=static_cast<Handle*>(handle.get())->event->signaled;const bool result=set;set=false;return result;}
void run(){
 currentPid=HostPid;fixture::clock=2000;auto client=worldframe::acquireClient(GamePid,800);client->subscribe(1,worldframe::Visual);
 CHECK(!client->publicationSignal()); // pre-notification Runtime fallback
 currentPid=GamePid;worldframe::Server server;failEventCreate=1;CHECK(server.wanted(HostPid,fixture::clock));
 currentPid=HostPid;CHECK(!client->publicationSignal());
 fixture::clock+=500;client->subscribe(1,worldframe::Visual);currentPid=GamePid;CHECK(server.wanted(HostPid,fixture::clock));
 currentPid=HostPid;CHECK(!client->publicationSignal());
 fixture::clock+=500;client->subscribe(1,worldframe::Visual);currentPid=GamePid;CHECK(server.wanted(HostPid,fixture::clock));
 currentPid=HostPid;auto signal=client->publicationSignal();CHECK(signal&&!take(signal));const auto opens=eventOpens;
 CHECK(client->publicationSignal()==signal&&eventOpens==opens);
 worldframe::Snapshot source;fillPages(source,2,1,800);currentPid=GamePid;server.observe(source.key);server.publish(source);
 CHECK(take(signal));const auto signals=eventSignals;for(unsigned n=0;n<1000;++n){server.observe(source.key);server.publish(source);}
 CHECK(eventSignals==signals&&!take(signal));
 currentPid=HostPid;CHECK(client->acquire(7)->key.serial==1);
 // Several publications before the consumer wakes collapse into one signal;
 // the immutable token comes from the latest sealed descriptor.
 currentPid=GamePid;for(unsigned serial=2;serial<=10;++serial){fillPages(source,2,serial,800);server.observe(source.key);server.publish(source);}
 CHECK(take(signal)&&!take(signal));currentPid=HostPid;CHECK(client->acquire(7)->key.serial==10);
 currentPid=GamePid;server.retire();CHECK(take(signal));server.retire();CHECK(!take(signal));
 currentPid=HostPid;CHECK(!client->acquire(7));auto replacement=worldframe::acquireClient(GamePid,801);replacement->subscribe(1,worldframe::Visual);CHECK(!replacement->publicationSignal());
 currentPid=GamePid;CHECK(server.wanted(HostPid,fixture::clock));fillPages(source,2,11,801);server.observe(source.key);server.publish(source);
 currentPid=HostPid;auto next=replacement->publicationSignal();CHECK(next&&next!=signal&&take(next)&&!take(signal));CHECK(!client->publicationSignal());CHECK(replacement->acquire(7)->key.serial==11);
 currentPid=GamePid;server.disconnect();CHECK(take(next));currentPid=HostPid;CHECK(!replacement->acquire(7));
 std::printf("PASS publication events: %zu assertions; capability fallback/retry, no duplicate wakes, coalescing, retirement, connection replacement and pinned handles\n",checks);
}
}
int main(){notifications::run();}
