#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/watch_slots.h"
#include "../source/watch_world_lifetime.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace external;
static std::vector<uint8_t> memory(0xC00000);static uint32_t failAt=0;static unsigned checks=0;
template<class T>static void put(uint32_t p,T v){std::memcpy(memory.data()+p,&v,sizeof v);}
static bool read(uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size()||(failAt>=p&&failAt<uint64_t(p)+n))return false;std::memcpy(out,memory.data()+p,n);return true;}
static void check(bool okay){++checks;assert(okay);}
static watch::Target actor(uint32_t p,uint32_t id,int x=1000,int y=1000){put(p,0x7F5C70u);put(p+0x10,id);put(p+0x6C,100);put(p+0x74,uint8_t(1));put(p+0x90,uint8_t(1));put(p+0x9C,x);put(p+0xA0,y);return {p,id};}
// Extract the actual Host synchronization entry so the source-epoch fallback
// regression is checked through its real mutation order, not a model alone.
struct Data {int status=1;uint32_t match=7,sampledTick=1;} data,*externalData=&data;
struct Reader {bool readFresh(uint32_t p,void*out,size_t n){return read(p,out,n);}}externalReader;
static uint32_t process=0,pid=123,epoch=0,controls=0,lifetimeSample=0,now=1;
static bool lifetimeReady=false;static watchworld::Lifetime lifetime;
struct Worker {unsigned invalidates=0;void invalidate(){++invalidates;}}worker;
struct Applied {void reset(){}}applied;
static watch::Track hostTrack;static unsigned resets=0;
static uint32_t sampleRenderClock(void*){return now;}
static void reset(){++resets;hostTrack={};lifetime.reset();lifetimeSample=0;lifetimeReady=false;}
#include "watch_lifetime_entry_extract.h"
int main(){
 auto a=actor(0x20000,1);watch::Track original;original.target=a;original.members={a};original.located=true;original.x=1000;original.y=1000;original.spatial=a;
 auto track=original;auto s=watch::life(read,a);check(s.state==watch::Life::Present&&s.drawable());
 // Distinct true values are essential: equal Alive/OnMap masks the old swap.
 put(a.pointer+0x74,uint8_t(0));s=watch::life(read,a);check(s.state==watch::Life::Present&&!s.drawable()&&s.alive&&!s.onMap);
 watch::follow(read,track);check(track.target==a&&track.located&&track.x==1000);
 failAt=a.pointer+0x90;watch::follow(read,track);check(track.target==a);failAt=0;
 put(a.pointer+0x74,uint8_t(1));put(a.pointer+0x9C,1200);watch::follow(read,track);check(track.target==a&&track.x==1200);
 put(a.pointer+0x90,uint8_t(0));check(watch::life(read,a).state==watch::Life::Dead);watch::follow(read,track);check(!track);
 // A verified nearby member may succeed; a reused pointer never takes over.
 actor(a.pointer,a.id);auto b=actor(0x30000,2,1100,1000);track=original;track.members.push_back(b);put(a.pointer+0x6C,0);watch::follow(read,track);check(track.target==b);
 track=original;put(a.pointer+0x10,99u);watch::follow(read,track);check(!track);
 actor(a.pointer,a.id);track=original;put(a.pointer+0x81,uint8_t(1));put(a.pointer+0x74,uint8_t(0));watch::follow(read,track);check(track.target==a);
 put(0xA8B230,0x600000u);put(0xA8B238,3);put(0x87F924,0x700000u);put(0x87F928,262144);put(0xA8ED84,100);put(0xA8E9A0,uint8_t(1));
 check(synchronizeWorld()&&epoch==7&&resets==1);hostTrack=original;
 // The external reader can become unknown, then advance its own serial on
 // recovery in the SAME Scenario/map. Neither path discards a manual pin.
 ++now;data.sampledTick=now;data.status=0;check(!synchronizeWorld()&&hostTrack.target==a);
 ++now;data.sampledTick=now;data.status=1;data.match=8;check(synchronizeWorld()&&epoch==8&&hostTrack.target==a&&resets==1);
 ++now;data.sampledTick=now;failAt=0x87F924;check(!synchronizeWorld()&&hostTrack.target==a);failAt=0;
 ++now;data.sampledTick=now;check(synchronizeWorld()&&hostTrack.target==a);
 // Focus/pause do not participate in world identity. A real rewind does.
 put(0xA8E9A0,uint8_t(0));++now;data.sampledTick=now;check(synchronizeWorld()&&hostTrack.target==a);
 put(0xA8ED84,99);++now;data.sampledTick=now;check(synchronizeWorld()&&!hostTrack&&resets==2);
 hostTrack=original;put(0xA8B230,0x610000u);++now;data.sampledTick=now;check(synchronizeWorld()&&!hostTrack&&resets==3);
 hostTrack=original;pid=456;check(synchronizeWorld()&&!hostTrack&&resets==4);
 hostTrack=original;pid=0;check(!synchronizeWorld()&&!hostTrack&&resets==5);
 std::printf("PASS production watch lifecycle and Host synchronization: %u checks; off-map alive, unknown reads, real death/succession/reuse, display epoch recovery, rewind/world/PID retirement.\n",checks);
}
