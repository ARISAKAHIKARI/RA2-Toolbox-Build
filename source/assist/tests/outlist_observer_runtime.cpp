#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "../source/outlist_observer.h"
struct SharedMock {bool nativeMegaGateway=true;int nativeMegaGatewayStatus=0;uint32_t nativeMegaGatewayAccepted=0,nativeMegaGatewaySkipped=0;};
static SharedMock shared;static SharedMock*sh=&shared;
struct SnapMock {int status=1,commandStatus=1,session=3;uint32_t queueSent=0;};
static SnapMock snap;
static uint32_t match=1,lastHouse=0x20000;
static bool ready=true,failRead=false,tearHeader=false;
static unsigned reads=0;
static assist::outlistobserve::Snapshot game;
static bool riskReady(){return ready;}
namespace eventdelivery {static bool isMultiplayer(int s){return s==3||s==4;}}
static bool read(uint32_t at,void*out,size_t n){
 ++reads;if(failRead)return false;
 auto copy=[&](const void*p,size_t bytes){if(n!=bytes)return false;memcpy(out,p,n);return true;};
 switch(at){
 case 0xA8B230:return copy(&game.identity.scenario,4);
 case 0xA83D4C:return copy(&game.identity.house,4);
 case 0x20010:return copy(&game.identity.houseID,4);
 case 0x20030:return copy(&game.identity.houseIndex,4);
 case 0xA8B238:return copy(&game.identity.session,4);
 case 0xA8ED84:return copy(&game.frame,4);
 case 0xA802C8:{auto r=game.ring;if(tearHeader&&reads>8)r.count=99;return copy(&r,sizeof r);}
 case 0xA802D4:return copy(game.events,sizeof game.events);
 case 0xA83A54:return copy(game.timings,sizeof game.timings);
 default:return false;
 }
}
#include "../source/native_mega_gateway_bridge.h"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static void tick(){auto original=game;reads=0;nativeMegaGatewayTick();CHECK(!memcmp(&game,&original,sizeof game));}
static void append(uint32_t actor,uint8_t house=0){
 auto*p=game.events[game.ring.tail];memset(p,0,111);p[0]=4;p[2]=house;uint32_t f=100;
 memcpy(p+3,&f,4);memcpy(p+7,&actor,4);p[11]=0x34;p[12]=2;
 game.timings[game.ring.tail]=actor;game.ring.tail=(game.ring.tail+1)&127;++game.ring.count;
}
int main(){
 game.identity={1,0x10000,0x20000,7,3,0};game.frame=100;
 tick();CHECK(nativeMegaGatewayActive&&shared.nativeMegaGatewayStatus==1&&shared.nativeMegaGatewayAccepted==0);
 append(99);tick();CHECK(shared.nativeMegaGatewayAccepted==1);
 tick();CHECK(shared.nativeMegaGatewayAccepted==1);
 append(100);++snap.queueSent;tick();CHECK(shared.nativeMegaGatewayAccepted==1&&shared.nativeMegaGatewaySkipped==1);
 append(101,1);tick();CHECK(shared.nativeMegaGatewayAccepted==1);
 append(102);tick();CHECK(shared.nativeMegaGatewayAccepted==2);
 failRead=true;tick();CHECK(!nativeMegaGatewayActive&&shared.nativeMegaGatewayStatus==-3);
 failRead=false;tick();CHECK(shared.nativeMegaGatewayAccepted==2); // new baseline, no replay
 tearHeader=true;tick();CHECK(shared.nativeMegaGatewayStatus==-3);tearHeader=false;
 snap.session=5;tick();CHECK(shared.nativeMegaGatewayStatus==-4&&!nativeMegaGatewayActive);
 snap.session=3;ready=false;tick();CHECK(shared.nativeMegaGatewayStatus==0);
 ready=true;shared.nativeMegaGateway=false;tick();CHECK(shared.nativeMegaGatewayStatus==0);
 shared.nativeMegaGateway=true;tick();CHECK(shared.nativeMegaGatewayAccepted==2);
 CHECK(nativeMegaGatewayAction(11,0,0)==0);
 sh=nullptr;tick();CHECK(!nativeMegaGatewayActive);
 std::printf("PASS: %u integration checks; native bridge diagnostic-only, game snapshots byte-identical\n",checks);
}
