// Production display/collector/ProcessReader/IPC with synthetic game bytes.
#include "protocol.h"
#include "../../shared/source/world_frame_capture.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <climits>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include "host_platform.h"
using namespace assist;
static external::ProcessReader externalReader;
static worldframe::Server frameServer;
static fixture::Memory memory;
static Snapshot snap{};
static Snapshot*externalData=&snap;
static uint32_t pid=fixture::GamePid,match=7,serial=0,optionFlags=0;
static bool enabled=true;
static struct {bool passengerBubbles=true,dangerPassengers=false;}extra;
static uint32_t flags(){return optionFlags;}
namespace nativeviewhost {
struct Transform {bool active=false;uint32_t generation=1;struct {int w=0,h=0;}logical;};
struct Area {int x,y,w,h;};
static Area clip(const Transform&,int w,int h){return {0,0,w,h};}
static uint64_t key(const Transform&t){return t.generation;}
static bool localPoint(const Transform&,float&,float&){return true;}
}
#include "passenger_production.h"
#include "passenger_demand.h"
constexpr uint32_t Carrier=fixture::ActorA,Passenger=fixture::Passenger,CarrierType=fixture::Type;
constexpr uint32_t PassengerType=0x310100,Enemy=0x130000,PassengerName=0x700100;
static worldframe::Token token;
static void setup(){
 fixture::currentPid=fixture::HostPid;fixture::gameMemory=&memory;
 CHECK(externalReader.open(fixture::GamePid));snap.width=800;snap.height=600;snap.frame=100;snap.status=1;snap.match=match;
 memory.put(0xA83D4C,fixture::House);memory.put(0xA8B230,fixture::Scenario);memory.put(0xB0CD48,0.0866025403784);
 memory.vector(0xA8EC78,fixture::TechnoList,{Carrier});memory.vector(0xA8EB40,fixture::BuildingList,{});
 memory.vector(0xA80228,fixture::HouseList,{fixture::House,Enemy});memory.put(Enemy+0x30,1);
 memory.put(Carrier,0x7F5C70u);memory.put(Carrier+0x6C,100);memory.put(Carrier+0x74,uint8_t(1));memory.put(Carrier+0x90,uint8_t(1));memory.put(Carrier+0x10,42u);
 memory.put(Carrier+0x21C,Enemy);memory.put(Carrier+0x6C4,CarrierType);memory.put(Carrier+0x431,uint8_t(1));
 memory.put(Carrier+0x9C,1024);memory.put(Carrier+0xA0,512);memory.put(Carrier+0x114,1);memory.put(Carrier+0x118,Passenger);
 memory.put(Passenger,0x7EB058u);memory.put(Passenger+0x6C,100);memory.put(Passenger+0x11C,Carrier);memory.put(Passenger+0x6C0,PassengerType);
 // One ASCII character also has the same representation in the Linux wchar
 // test seam; no claim is made about rendering Windows UTF-16 here.
 memory.put(PassengerType+0x60,PassengerName);memory.put(PassengerName,uint16_t('G'));
 memory.put(PassengerType+0x898,fixture::Weapon);memory.put(fixture::Weapon+0x144,uint8_t(1));
 memory.put(fixture::Weapon+0xB4,2048);memory.put(fixture::Weapon+0xA0,fixture::Projectile);memory.put(fixture::Weapon+0xAC,fixture::Warhead);
}
// One actual producer capture per simulated logic frame. Consumers use the
// real shared-memory client to acquire the exact publication subsequently.
static void publish(uint32_t omittedPage=0,bool failedPage=false){
 CHECK(!externalReader.worldFrameActive());configureDemand();externalReader.beginWorldFrame(match);externalReader.endWorldFrame();
 fixture::currentPid=fixture::GamePid;CHECK(frameServer.wanted(fixture::HostPid,fixture::clock));
 auto key=fixture::key(++snap.frame);key.match=match;key.serial=++serial;key.tick=fixture::clock;key.demand=externalReader.worldDemand();
 key.connectionLow=uint32_t(frameServer.connection());key.connectionHigh=uint32_t(frameServer.connection()>>32);key.generationLow=77;
 worldframe::Snapshot frame;frame.reset(key);if(failedPage)memory.denied.push_back({omittedPage,4096});
 worldframe::CaptureGraph<fixture::Read> graph(frame,fixture::Read{&memory});CHECK(graph.capture(key.demand,{}));CHECK(frame.seal());memory.denied.clear();
 worldframe::Snapshot filtered;const worldframe::Snapshot*chosen=&frame;
 if(omittedPage&&!failedPage){std::vector<worldframe::Page> pages;for(size_t i=0;i<frame.pages.size();++i)if(frame.pages[i].address!=omittedPage)pages.push_back(frame.pages[i]);CHECK(filtered.import(key,frame.counters,pages.data(),pages.size()));chosen=&filtered;}
 frameServer.observe(key);frameServer.publish(*chosen);fixture::currentPid=fixture::HostPid;
 CHECK(externalReader.beginWorldFrame(match));token=externalReader.worldFrameToken();externalReader.endWorldFrame();
 snap.match=match;snap.sampledTick=key.tick;externalData=&snap;externalReader.begin();memory.calls.clear();fixture::rpmCalls=0;
}
static void observed(){
 // A real paused Host keeps its subscription alive while the producer makes
 // an independent observation; heartbeat alone cannot authenticate the data.
 externalReader.beginWorldFrame(match,token);externalReader.endWorldFrame();
 fixture::currentPid=fixture::GamePid;frameServer.observe(token->key);fixture::currentPid=fixture::HostPid;
}
static void zeroLiveReads(){CHECK(fixture::rpmCalls==0&&memory.calls.empty());}
int main(){
 std::setvbuf(stdout,nullptr,_IONBF,0);
 setup();publish();CHECK(externalReader.worldDemand()==(worldframe::Base|worldframe::Passengers|worldframe::Combat));
 auto first=passengerSample(fixture::clock,token);std::printf("first passenger paint: bubbles=%zu live_RPM=%zu\n",first.size(),fixture::rpmCalls);std::fflush(stdout);
 CHECK(first.size()==1&&first[0].id==42&&first[0].danger);zeroLiveReads();
 // A live value changes after capture. Rendering the sealed frame must retain
 // its observed position while avoiding any process read at display cadence.
 memory.put(Carrier+0x9C,8000);memory.put(PassengerType+0x60,0u);
 for(unsigned i=0;i<1000;++i){++fixture::clock;externalReader.begin();auto next=passengerSample(fixture::clock,token);CHECK(next.size()==1&&next[0].left==first[0].left);}
 zeroLiveReads();CHECK(token->key.serial==serial);
 std::puts("PASS 1000 paints of one logic frame: zero live RPM, immutable contents/position");
 memory.put(Carrier+0x9C,1024);memory.put(PassengerType+0x60,PassengerName);clearPassengerDisplay();
 fixture::clock=2000;publish();CHECK(passengerSample(2000,token).size()==1);
 fixture::clock=2020;publish(Passenger&~4095u);CHECK(passengerSample(2020,token).size()==1);zeroLiveReads();
 CHECK(passengerSample(2119,token).size()==1);CHECK(passengerSample(2120,token).empty());zeroLiveReads();
 fixture::clock=2140;publish(Passenger&~4095u,true);CHECK(passengerSample(2140,token).empty());zeroLiveReads();
 fixture::clock=2160;publish();CHECK(passengerSample(2160,token).size()==1);zeroLiveReads();
 std::puts("PASS missing/failed passenger page: no live fallback, 120ms lease never renewed, next complete frame recovers");
 CHECK(passengerSample(2161,{}).empty());CHECK(passengerSample(2162,token,false).empty());zeroLiveReads();
 ++snap.frame;CHECK(passengerSample(2163,token).empty());--snap.frame;
 ++snap.match;CHECK(passengerSample(2164,token).empty());--snap.match;
 ++snap.sampledTick;CHECK(passengerSample(2165,token).empty());--snap.sampledTick;
 ++pid;CHECK(passengerSample(2166,token).empty());--pid;zeroLiveReads();
 CHECK(passengerSample(2167,token).size()==1);snap.chat=1;CHECK(passengerSample(2168,token).empty());snap.chat=0;
 enabled=false;CHECK(passengerSample(2169,token).empty());enabled=true;zeroLiveReads();
 fixture::currentPid=fixture::GamePid;frameServer.retire();fixture::currentPid=fixture::HostPid;CHECK(passengerSample(2170,token).empty());zeroLiveReads();
 std::puts("PASS no token/unconfirmed/wrong PID/match/frame/tick/retired publication/disabled/chat: closed without RPM");
 fixture::clock=2200;publish();CHECK(passengerSample(2200,token).size()==1);
 memory.put(Carrier+0x431,uint8_t(0));fixture::clock=2220;publish();CHECK(passengerSample(2220,token).empty());zeroLiveReads();
 extra.passengerBubbles=false;extra.dangerPassengers=true;fixture::clock=2240;publish();auto danger=passengerSample(2240,token);CHECK(danger.size()==1&&danger[0].danger);zeroLiveReads();
 extra.dangerPassengers=false;configureDemand();CHECK(externalReader.worldDemand()==worldframe::Base);CHECK(passengerSample(2241,token).empty());zeroLiveReads();
 optionFlags=Capacity;configureDemand();CHECK(externalReader.worldDemand()==(worldframe::Base|worldframe::Passengers));optionFlags=0;
 std::puts("PASS inspect disappearance/danger-only/shared demand routing: no rate reduction or alternate live scanner");
 // A paused, independently re-observed frame is still the same immutable
 // publication; this does not refresh its acquisition tick.
 extra.passengerBubbles=true;extra.dangerPassengers=false;memory.put(Carrier+0x431,uint8_t(1));fixture::clock=2300;publish();const auto acquired=token->key.tick;
 fixture::clock=6000;observed();CHECK(passengerSample(6000,token).size()==1&&token->key.tick==acquired);zeroLiveReads();
 fixture::clock=7001;CHECK(passengerSample(7001,token).empty());zeroLiveReads();
 std::puts("PASS paused frame independent confirmation and confirmation expiry");
 std::printf("passenger frame sampling: %zu checks passed\n",fixture::checks);
}
