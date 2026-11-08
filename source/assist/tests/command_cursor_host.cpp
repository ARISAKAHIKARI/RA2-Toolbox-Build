#include <cassert>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include "../../shared/source/world_frame.h"
#include <iostream>
#include "../source/protocol.h"
#include "../source/additional_settings.h"
#include "../../external_reader/source/game_view.h"
#include "../../external_reader/source/display_state.h"
#include "../../shared/source/command_cursor_hint.h"
using namespace assist;
static std::map<uint32_t,uint8_t> memory;
template<class T>void put(uint32_t a,T v){auto*b=(uint8_t*)&v;for(size_t n=0;n<sizeof v;++n)memory[a+n]=b[n];}
void zero(uint32_t a,int n){for(int i=0;i<n;++i)memory[a+i]=0;}
static worldframe::Token latestWorld;static bool freezeWorld=false;static uint32_t failLive=0,worldSerial=0,lastDemand=0;static size_t liveWorldReads=0;
struct Reader {
 worldframe::Token pinned;uint32_t demand=worldframe::Base;bool complete=true;
 uint32_t pid()const{return 77;}uint64_t generation()const{return 1;}bool open(uint32_t){return true;}void begin(){}
 bool beginWorldFrame(uint32_t match,worldframe::Token token={}){pinned=token?token:latestWorld;complete=true;lastDemand=demand;return pinned&&pinned->key.match==match;}
 void endWorldFrame(){pinned.reset();}worldframe::Token worldFrameToken()const{return pinned;}bool worldFrameComplete()const{return complete;}
 void markWorldIncomplete(){complete=false;}uint32_t worldDemand()const{return demand;}void setWorldDemand(uint32_t value){demand=value|worldframe::Base;}
 bool read(uint32_t a,void*out,size_t n){if(!n)return true;if(pinned&&pinned->read(a,out,n))return true;complete=false;return false;}
 bool readFresh(uint32_t a,void*out,size_t n){return read(a,out,n);}
 bool readLive(uint32_t a,void*out,size_t n){if(a==failLive)return false;
  if(a==0x20000+0x21C||a==0x20000+0x6C||a==0x50000+0xB4||a==0x90000+0x114)++liveWorldReads;
  auto*b=(uint8_t*)out;for(size_t i=0;i<n;++i){auto p=memory.find(a+i);if(p==memory.end())return false;b[i]=p->second;}return true;
 }
}externalReader;
namespace external {using ReadSession=Reader;}
static auto storage=std::make_unique<Shared>();static Shared*shared=storage.get();
static bool enabled=true,focused=true,fresh=true;static uint32_t pid=77,clockNow=1000,featureFlags=AdvancedInput|AircraftSpread|CustomCS;
static uint8_t keys[256]{};static Snapshot incoming;static AdditionalSettings extra;
static int hook=0;namespace gameinput {static void keySnapshot(int,uint8_t*out){std::memcpy(out,keys,256);}}
static featuretypes::Config featureTypes;
static uint32_t advancedKey='G'|(4<<8),magnetronKey='M'|(4<<8),csHoldKey='G'|(2<<8);
static uint32_t settingsRevision=1;
static uint32_t GetTickCount(){return clockNow;}
static short GetAsyncKeyState(int n){return keys[n]?short(0x8000):0;}
static bool commandFresh(uint32_t){return fresh;}static bool foregroundGame(){return focused;}
static bool formationInputOwns(){return false;}static bool runtimeMiddleOwnsNow(){return false;}
static uint32_t commandMatch(){return 8;}static uint32_t flags(){return featureFlags;}
namespace airway {bool held(const uint8_t*){return false;}}
#include "../source/command_cursor_host.h"
static constexpr uint32_t unit=0x20000,type=0x30000,house=0x40000,weapon=0x50000,projectile=0x60000,warhead=0x70000;
void fixture(){
 memory.clear();freezeWorld=false;failLive=0;latestWorld.reset();liveWorldReads=0;std::memset(keys,0,sizeof keys);featureTypes={};extra={};enabled=focused=fresh=true;incoming={};
 featureFlags=AdvancedInput|AircraftSpread|CustomCS;advancedKey='G'|(4<<8);clockNow=1000;
 for(auto a:{unit,type,house,weapon,projectile,warhead})zero(a,0x2000);
 put(house+0x5788,0u);put(0xA83D4C,house);put(0xA8ED84,100);put(0xA8B230,0x900000u);put(0x87F7E8+0x13C,0xA00000u);put<uint8_t>(0xA8E378,1);put(0xA8EDA0,0);put(0xA8ED8C,0);zero(0x87F7E8+0x11B0,5);
 put(0xA8ECB8+4,0x80000);put(0xA8ECB8+8,1);put(0xA8ECB8+16,1);put(0x80000,unit);
 put(unit,0x7f5c70);put(unit+0x10,71u);put(unit+0x21c,house);put(unit+0x6c4,type);put(unit+0x6c,100);put<uint8_t>(unit+0x74,1);put<uint8_t>(unit+0x90,1);
 const char id[]="TANK";for(size_t n=0;n<sizeof id;++n)put(type+0x24+n,id[n]);
 put(type+0x898,weapon);put(weapon+0xb4,8*256);put(weapon+0xa0,projectile);put(weapon+0xac,warhead);put<uint8_t>(projectile+0x2a5,1);
 commandcursor::clear();
}
static worldframe::Token sealWorld(bool missingObject=false){
 auto result=std::make_shared<worldframe::Snapshot>();worldframe::Key key;key.pid=77;key.owner=9;key.match=8;key.serial=++worldSerial;key.tick=clockNow;key.scenario=0x900000;key.cells=0xA00000;key.house=house;key.cellCount=512*512;key.connectionLow=1;key.generationLow=2;
 int frame=0;for(unsigned i=0;i<4;++i)frame|=int(memory[0xA8ED84+i])<<(i*8);key.frame=frame;result->reset(key);
 std::set<uint32_t>pages;for(const auto&byte:memory)pages.insert(byte.first&~4095u);
 auto source=[](uint32_t a,void*out,size_t n){auto*b=(uint8_t*)out;for(size_t i=0;i<n;++i){auto p=memory.find(a+uint32_t(i));b[i]=p==memory.end()?0:p->second;}return true;};
 for(auto page:pages)if(!missingObject||page!=unit)assert(result->prefetch(source,page,4096));assert(result->seal());return result;
}
commandcursor::Mode mode(){clockNow+=33;if(!freezeWorld){int frame=0;for(unsigned i=0;i<4;++i)frame|=int(memory[0xA8ED84+i])<<(i*8);put(0xA8ED84,frame+1);latestWorld=sealWorld();}serviceCommandCursorHint();return commandcursor::read(pid,clockNow).mode;}

int main(){
 using namespace commandcursor;
 fixture();keys['G']=keys[18]=1;assert(mode()==Advanced);
 advancedKey|=0x8000;assert(mode()==commandcursor::None);keys['G']=3;assert(mode()==Advanced);keys[18]=0;assert(mode()==commandcursor::None);keys[18]=1;advancedKey&=~0x8000;keys['G']=1;
 keys['G']=0;assert(mode()==commandcursor::None);keys['G']=1;assert(mode()==Advanced);
 fresh=false;assert(mode()==commandcursor::None);fresh=true;focused=false;assert(mode()==commandcursor::None);focused=true;
 incoming.chat=1;assert(mode()==commandcursor::None);incoming.chat=0;
 put(0xA8ED8C,1);assert(mode()==commandcursor::None);put(0xA8ED8C,0);
 for(int offset=0;offset<5;++offset){put<uint8_t>(0x87F7E8+0x11B0+offset,1);assert(mode()==commandcursor::None);put<uint8_t>(0x87F7E8+0x11B0+offset,0);}
 put(unit+0x21c,house+0x100);assert(mode()==commandcursor::None);put(unit+0x21c,house);
 put<uint8_t>(unit+0x1c8,1);assert(mode()==commandcursor::None);put<uint8_t>(unit+0x1c8,0);
 featureTypes.filters[featuretypes::AdvancedAttack].custom=1;assert(mode()==commandcursor::None);featureTypes={};
 extra.mouseButtons[pointerinput::MouseAdvanced]=0;assert(mode()==commandcursor::None);extra.mouseButtons[pointerinput::MouseAdvanced]=1;
 advancedKey=0;keys['G']=keys[18]=0;assert(mode()==commandcursor::None); // no permanent mouse-only hint
 fixture();keys[17]=1;put(unit,0x7e22a4);extra.airKey1=0;extra.airKey2=17;assert(mode()==AirSecond);extra.airKey2=0;extra.airKey1=17;assert(mode()==AirFirst);
 featureTypes.filters[featuretypes::Aircraft].custom=1;assert(mode()==commandcursor::None);featureTypes={};
 put(unit,0x7f5c70);assert(mode()==commandcursor::None);put(unit,0x7e22a4);
 assert(mode()==AirFirst);clockNow+=101;assert(read(pid,clockNow).mode==commandcursor::None);
 assert(read(pid+1,clockNow-101).mode==commandcursor::None);
 fixture();keys['G']=keys[17]=1;assert(mode()==CS);
 keys[18]=1;assert(mode()==commandcursor::None); // exact modifier matching
 fixture();featureFlags=AdvancedEnter;extra.enterKey=18;keys[18]=1;
 assert(mode()==commandcursor::None); // Flat ground never advertises entry.
 const uint32_t carrier=0x90000,carrierType=0xA0000;zero(carrier,0x2000);zero(carrierType,0x2000);
 put(0xA8EC78+4,0xB0000);put(0xA8EC78+8,1);put(0xA8EC78+16,1);put(0xB0000,carrier);
 put(carrier,0x7f5c70);put(carrier+0x10,81u);put(carrier+0x6c4,carrierType);put(carrier+0x6c,100);put<uint8_t>(carrier+0x74,1);put<uint8_t>(carrier+0x90,1);put<uint8_t>(carrier+0x431,1);put(carrier+0x21c,house);
 put(carrierType+0x5e0,4);put<double>(carrierType+0x388,3.);put<double>(type+0x380,2.);
 assert(mode()==Enter);freezeWorld=true;put<uint8_t>(carrier+0x431,0);assert(mode()==commandcursor::None);put<uint8_t>(carrier+0x431,1);assert(mode()==Enter&&lastDemand==(worldframe::Base|worldframe::Passengers));freezeWorld=false;
 put(carrier+0x21c,house+0x20000);put<int>(house+0x20000+0x30,1);assert(mode()==commandcursor::None);put(carrier+0x21c,house);
 put(carrierType+0x5e0,1);assert(mode()==commandcursor::None);put(carrierType+0x5e0,4);
 put(unit+0x11c,carrier);assert(mode()==commandcursor::None);put(unit+0x11c,0);
 featureTypes.filters[featuretypes::EnterTarget].custom=1;assert(mode()==commandcursor::None);featureTypes={};
 put(0xA8ECB8+16,0);assert(mode()==commandcursor::None);
 // Runtime world data stays fixed while paused; selected input remains live.
 fixture();keys['G']=keys[18]=1;assert(mode()==Advanced);freezeWorld=true;auto source=latestWorld;
 put(unit+0x21C,house+4);put(unit+0x6C,0);put(weapon+0xB4,0);assert(mode()==Advanced&&latestWorld==source&&lastDemand==(worldframe::Base|worldframe::Combat));
 put(0xA8ECB8+16,0);assert(mode()==commandcursor::None);put(0xA8ECB8+16,1);assert(mode()==Advanced);
 auto stable=read(pid,clockNow);failLive=0xA8ECB8+16;assert(mode()==Advanced&&read(pid,clockNow).tick==stable.tick);failLive=0;
 latestWorld=sealWorld(true);assert(mode()==Advanced&&read(pid,clockNow).tick==stable.tick);clockNow+=101;assert(mode()==commandcursor::None);
 latestWorld.reset();assert(mode()==commandcursor::None&&liveWorldReads==0);
 assert(ordinary(18)&&ordinary(71)&&!ordinary(30)&&!ordinary(79)&&!ordinary(1));
 for(auto m:{Advanced,AirFirst,AirSecond,CS,Magnetron,Enter,Disk}){
  assert(recolor(0,m,27,21,55,43)==0);assert(recolor(0xff000000,m,27,21,55,43)==0xff000000);
  assert(recolor(0xffeeeeee,m,27,21,55,43)==0xffeeeeee);
  assert((recolor(0x80700000,m,27,21,55,43)>>24)==0x80);
 }
 assert(recolor(0xff00ff00,Advanced,0,0,55,43)==0xffffff00);
 assert(recolor(0xffffff00,Advanced,0,0,55,43)==0xff00ff00);
 assert(recolor(0xffff0000,AirFirst,27,21,55,43)==0xff00ff00);
 assert(recolor(0xffff0000,AirFirst,27,4,55,43)==0xffff0000);
 std::cout<<"PASS: actual host service with real sealed Snapshot, paused world eligibility and live selected/hover input, zero live world fallback, missing sample/input retain bounded hint without timestamp renewal, Base/Combat/Passengers demand, selection/type filters, exact hold bindings, mouse-only suppression, native tools/menu/chat/focus, stale/wrong PID, original alpha/outlines and mode colors\n";
}
