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
static bool ready=true,failRead=false,failEvents=false,tearHeader=false,tearHouse=false;
static unsigned reads=0,bulkReads=0,headerReads=0,houseReads=0;
static assist::outlistobserve::Snapshot game;
static bool riskReady(){return ready;}
namespace eventdelivery {static bool isMultiplayer(int s){return s==3||s==4;}}
static bool read(uint32_t at,void*out,size_t n){
 ++reads;if(failRead)return false;
 auto copy=[&](const void*p,size_t bytes){if(n!=bytes)return false;memcpy(out,p,n);return true;};
 switch(at){
 case 0xA8B230:return copy(&game.identity.scenario,4);
 case 0xA83D4C:return copy(&game.identity.house,4);
 case 0x20010:{auto v=game.identity.houseID;if(tearHouse&&++houseReads>1)++v;return copy(&v,4);}
 case 0x20030:return copy(&game.identity.houseIndex,4);
 case 0xA8B238:return copy(&game.identity.session,4);
 case 0xA8ED84:return copy(&game.frame,4);
 case 0xA802C8:{auto r=game.ring;if(tearHeader&&++headerReads>1)r.count=99;return copy(&r,sizeof r);}
 case 0xA802D4:++bulkReads;return !failEvents&&copy(game.events,sizeof game.events);
 case 0xA83A54:++bulkReads;return copy(game.timings,sizeof game.timings);
 default:return false;
 }
}
#include "../source/native_mega_gateway_bridge.h"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static void tick(){auto original=game;reads=bulkReads=headerReads=houseReads=0;nativeMegaGatewayTick();CHECK(!memcmp(&game,&original,sizeof game));}
static void advance(){++game.frame;tick();}
static void append(uint32_t actor,uint8_t house=0){
 auto*p=game.events[game.ring.tail];memset(p,0,111);p[0]=4;p[2]=house;uint32_t f=uint32_t(game.frame);
 memcpy(p+3,&f,4);memcpy(p+7,&actor,4);p[11]=0x34;p[12]=2;
 game.timings[game.ring.tail]=actor;game.ring.tail=(game.ring.tail+1)&127;++game.ring.count;
}
int main(){
 game.identity={1,0x10000,0x20000,7,3,0};game.frame=100;
 tick();CHECK(nativeMegaGatewayActive&&shared.nativeMegaGatewayStatus==1&&shared.nativeMegaGatewayAccepted==0&&bulkReads==2);
 append(99);tick();CHECK(!bulkReads&&shared.nativeMegaGatewayAccepted==0);
 advance();CHECK(shared.nativeMegaGatewayAccepted==1&&bulkReads==2);
 for(unsigned i=0;i<10000;++i){tick();CHECK(!bulkReads&&shared.nativeMegaGatewayAccepted==1);}
 append(100);++snap.queueSent;advance();CHECK(shared.nativeMegaGatewayAccepted==1&&shared.nativeMegaGatewaySkipped==1);
 append(101,1);advance();CHECK(shared.nativeMegaGatewayAccepted==1);
 append(102);advance();CHECK(shared.nativeMegaGatewayAccepted==2);
 // Failed bulk attempts cannot retry the large read repeatedly in one frame.
 failEvents=true;advance();CHECK(!nativeMegaGatewayActive&&shared.nativeMegaGatewayStatus==-3&&bulkReads==1);
 const auto skipped=shared.nativeMegaGatewaySkipped;
 for(unsigned i=0;i<1000;++i){tick();CHECK(!bulkReads&&shared.nativeMegaGatewaySkipped==skipped&&shared.nativeMegaGatewayStatus==-3);}
 failEvents=false;advance();CHECK(shared.nativeMegaGatewayAccepted==2&&bulkReads==2);
 tearHeader=true;advance();CHECK(shared.nativeMegaGatewayStatus==-3);tearHeader=false;
 advance();CHECK(shared.nativeMegaGatewayAccepted==2);
 tearHouse=true;advance();CHECK(shared.nativeMegaGatewayStatus==-3);tearHouse=false;
 advance();CHECK(shared.nativeMegaGatewayAccepted==2);
 // Same numeric frame, different world identity: establish a new baseline now.
 ++game.identity.houseID;tick();CHECK(bulkReads==2&&shared.nativeMegaGatewayAccepted==2);
 tick();CHECK(!bulkReads);
 failRead=true;advance();CHECK(!bulkReads&&shared.nativeMegaGatewayStatus==-3);
 failRead=false;advance();CHECK(shared.nativeMegaGatewayAccepted==2);
 --game.frame;tick();CHECK(bulkReads==2&&shared.nativeMegaGatewayAccepted==2);
 snap.session=5;tick();CHECK(shared.nativeMegaGatewayStatus==-4&&!nativeMegaGatewayActive&&!bulkReads);
 snap.session=3;ready=false;tick();CHECK(shared.nativeMegaGatewayStatus==0&&!bulkReads);
 ready=true;shared.nativeMegaGateway=false;tick();CHECK(shared.nativeMegaGatewayStatus==0&&!bulkReads);
 shared.nativeMegaGateway=true;tick();CHECK(shared.nativeMegaGatewayAccepted==2&&bulkReads==2);
 CHECK(nativeMegaGatewayAction(11,0,0)==0);
 sh=nullptr;tick();CHECK(!nativeMegaGatewayActive&&!bulkReads);
 std::printf("PASS: %u integration checks; per-frame bulk budget, retry throttling, context checks, game snapshots byte-identical\n",checks);
}
