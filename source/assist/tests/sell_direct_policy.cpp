#include "command_intent.h"
#include "sell_packet.h"
#include "event_delivery.h"
#include <cassert>
#include <cstring>
#include <map>
#include <tuple>
#include <vector>
#include <iostream>
using namespace assist;using Event=vanilla::Packet;using DWORD=uint32_t;
enum class OrderAuthority{Local,SellPlayerTest};
static std::map<uint32_t,uint8_t> memory;
template<class T>T get(uint32_t at){T v{};for(unsigned i=0;i<sizeof(T);++i)reinterpret_cast<uint8_t*>(&v)[i]=memory[at+i];return v;}
template<class T>void put(uint32_t at,T v){for(unsigned i=0;i<sizeof(T);++i)memory[at+i]=reinterpret_cast<uint8_t*>(&v)[i];}
struct Shared {unsigned sellDirectPending=0,sellDirectSent=0,sellDirectRejected=0;int sellDirectStatus=0;} shared,*sh=&shared;
struct Snap {bool chat=false;unsigned queueSent=0,queuedEvents=0;} snap;
static int outgoingBudget=10;static uint32_t match=1,now=100,object=0x100000,owner=0x200000,id=7;static bool ready=true,authorized=true,sendOK=true,clockChange=false;
static unsigned sends=0,flushes=0;static std::vector<uint8_t> wire;
static struct Versions{uint32_t revision=0;uint32_t get(uint32_t)const{return revision;}}manualVersions;
static bool sellAllowed(uint32_t){return authorized;}
static bool simulationReady(){return ready;}
static uint32_t sampleClock(Shared*){return now;}
static uint32_t worldByID(uint32_t x){return x==id?object:0;}
static bool live(uint32_t x){return x==object;}
static bool authorizePlayerOrder(const intent::EventOrder&o,OrderAuthority){return authorized&&o.player==get<int>(owner+0x30);}
static bool sellHumanHouse(uint32_t h){return get<uint8_t>(h+0x1EC)||get<uint8_t>(h+0x1ED);}
static bool nativeExecutableRange(uint32_t,size_t){return true;}
static bool gameSendTiming(uint32_t&t){t=123;if(clockChange)put<int>(0xA8ED84,121);return true;}
template<class R,class...Args>R call(uint32_t at,uint32_t,Args...args){
 if constexpr(sizeof...(Args)==4){auto t=std::make_tuple(args...);assert(at==0x5414C0);++sends;wire.assign(std::get<0>(t),std::get<0>(t)+std::get<1>(t));return sendOK?1:0;}
 else{assert(at==0x541820);++flushes;return R();}
}
// Redirect only the production memcpy-to-native-address operation into this
// synthetic memory. Reads and writes above mirror the header's injected world.
static void* copy(void*dest,const void*src,size_t size){auto at=uintptr_t(dest);if(at<0x10000000){for(size_t i=0;i<size;++i)memory[uint32_t(at+i)]=static_cast<const uint8_t*>(src)[i];return dest;}return std::memcpy(dest,src,size);}
#define memcpy copy
#include "sell_direct_bridge.h"
#undef memcpy
static void init(int protocol=1){
 resetDirectSell();shared={};snap={};memory.clear();outgoingBudget=10;now=100;ready=authorized=sendOK=true;clockChange=false;sends=flushes=0;manualVersions.revision=0;
 put<int>(0xA8B238,3);put<int>(0xA8B24C,protocol);put<int>(0xA8ED84,120);put<int>(0xA8B550,15);put<int>(0xA8B554,5);put<uint32_t>(0xAC51FC,0x12345678);put<uint32_t>(0xB04474+120*4,0x12345678);
 put<uint32_t>(object+0x10,id);put<uint32_t>(object+0x21C,owner);put<int>(owner+0x30,1);put<uint32_t>(owner+0x10,99);put<uint8_t>(owner+0x1EC,1);
 put<uint32_t>(0xA8E9C0,0x7EC16C);put<uint32_t>(0x7EC16C+8,0x5414C0);put<uint32_t>(0x7EC16C+4,0x541820);put<uint8_t>(0xA8E9C4,1);put<uint8_t>(0xA8E9C5,1);put<int>(0xA8E9C0+0x44,2);
 put<int>(0x8B41F8,3);put<int>(0x8B4200,16383);put<uint16_t>(0xAFA400,65535);put<int>(0xA802C8,128); // full native OutList must not affect direct Sell
}
int main(){auto request=[](){return queueDirectSell(intent::object(1,0x16,id),OrderAuthority::SellPlayerTest,object);};
 for(int protocol=0;protocol<=2;++protocol){init(protocol);assert(request());assert(!request());serviceDirectSell();assert(sends==1&&flushes==1&&shared.sellDirectSent==1&&!directSellPending());assert(get<int>(0x8B4200)==0&&get<int>(0x8B41F8)==4&&get<int>(0xA802C8)==128);assert(get<uint16_t>(0xAFA400)==(protocol?0:65535));assert(get<uint8_t>(0x8B4204+16383*111)==0x16);assert(get<uint32_t>(0x8B4204+16383*111+3)==135);serviceDirectSell();assert(sends==1);}
 init();assert(request());sendOK=false;serviceDirectSell();assert(sends==1&&!flushes&&directSellPending()&&get<int>(0x8B41F8)==3&&get<uint16_t>(0xAFA400)==65535);sendOK=true;now+=51;serviceDirectSell();assert(shared.sellDirectSent==1&&!directSellPending());
 for(int mode=0;mode<7;++mode){init();assert(request());switch(mode){case 0:now+=3001;break;case 1:++manualVersions.revision;break;case 2:put<uint32_t>(object+0x21C,0x300000);break;case 3:put<uint8_t>(owner+0x1EC,0);break;case 4:put<int>(0xA8B238,5);break;case 5:authorized=false;break;case 6:clockChange=true;break;}serviceDirectSell();assert(!sends&&shared.sellDirectRejected==1&&!directSellPending());}
 init();assert(request());put<uint32_t>(0xB04474+120*4,0);serviceDirectSell();assert(!sends&&directSellPending());resetDirectSell();assert(!directSellPending());
 init(2);put<int>(0xA8ED84,121);assert(request());serviceDirectSell();assert(!sends&&directSellPending());
 init();put<uint8_t>(owner+0x1EC,0);assert(!request());init();for(int i=0;i<16;++i){id=7+i;assert(request());}id=99;assert(!request());
 std::cout<<"PASS direct Sell production service: 3 protocols, native OutList full, transport refusal/retry, serial wrap, local frame, stale/AI gates, CRC wait, bounded queue\n";
}
