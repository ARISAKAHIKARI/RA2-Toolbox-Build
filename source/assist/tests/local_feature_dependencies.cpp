// Unchanged production selection, landing and player transactions. Engine
// memory, native calls and the disabled player-switch product flag are explicit
// controlled boundaries; no game or Windows process is involved.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <vector>
#include <type_traits>
using DWORD=uint32_t;
struct POINT{int x=0,y=0;};
static void* GetModuleHandleW(const wchar_t*){return nullptr;}
namespace assist{}
using namespace assist;
static std::vector<uint8_t> memory(0xC00000);
static unsigned reads=0,writes=0,nativeCalls=0,unselections=0,selections=0,resets=0,redraws=0;
static std::map<uint32_t,unsigned> calls;
static std::set<uint32_t> unavailable;
static bool nativeDependencyReady(uint32_t address){return !unavailable.count(address);}
template<class...More>static bool nativeDependenciesReady(uint32_t first,More...rest){return nativeDependencyReady(first)&&(nativeDependencyReady(rest)&&...);}
template<class T>static T get(uint32_t at){assert(uint64_t(at)+sizeof(T)<=memory.size());++reads;T value{};memcpy(&value,memory.data()+at,sizeof value);return value;}
template<class T>static void put(uint32_t at,T value){assert(uint64_t(at)+sizeof(T)<=memory.size());++writes;memcpy(memory.data()+at,&value,sizeof value);}
static bool read(uint32_t at,void*out,size_t n){++reads;if(uint64_t(at)+n>memory.size())return false;memcpy(out,memory.data()+at,n);return true;}
struct Vec{uint32_t items=0;int count=0,capacity=0;};
static Vec vec(uint32_t address,int=16384){return get<Vec>(address);}
static uint32_t item(Vec value,int index){assert(index>=0&&index<value.count);return get<uint32_t>(value.items+uint32_t(index)*4);}
static void vectorAt(uint32_t at,uint32_t items,std::initializer_list<uint32_t> values){int index=0;for(auto value:values)put(items+4*index++,value);put(at,Vec{items,index,index});}
static std::map<uint32_t,int> kinds;
static std::set<uint32_t> worldObjects;
static int kind(uint32_t p){auto at=kinds.find(p);return at==kinds.end()?-1:at->second;}
static bool own(uint32_t p,uint32_t h){return p&&get<uint32_t>(p+0x21C)==h;}
static bool live(uint32_t p){return p&&get<int>(p+0x6C)>0;}
static uint32_t typeOf(uint32_t p,int){return get<uint32_t>(p+0x6C4);}
static uint32_t lastHouse=0x100000;
static constexpr int MaxTypes=512;
static struct{int typeCount=0;struct Type{int kind=0,index=0,naval=0;}types[MaxTypes];}snap;
static uint32_t typePointers[MaxTypes]{};
static struct Shared{bool magnetronDrop=true,switchPlayerEnabled=true,stop=false;}shared,*sh=&shared;
static bool playerContextChanged=false;
namespace playertest{static bool SwitchAvailable=false;}
static bool playableHouse(uint32_t p){return p==lastHouse||p==lastHouse+0x10000;}
static constexpr uint32_t Tabs=0x700000,Sidebar=0x710000,Actor=0x200000,Other=0x210000,Type=0x300000,Cell=0x400000;
static void clearFlashes(){}
static void sidebarRedraw(int){++redraws;}
static void reset(){++resets;}
static void unselect(){assert(nativeDependencyReady(0x48DC90));++unselections;}
static bool selection(uint32_t){assert(nativeDependencyReady(0x6FBFA0));++selections;return true;}
static uint32_t techFactory(uint32_t,uint32_t){assert(nativeDependenciesReady(0x5F7900,0x4F7870));return 0x500000;}
static int typeTab(int){assert(nativeDependencyReady(0x6ABC60));return 0;}
template<class R,class...Args>static R call(uint32_t address,uint32_t,Args...){
 ++nativeCalls;++calls[address];assert(nativeDependencyReady(address));
 if(address==0x578080)return R(0);
 assert(address==0x6D2140||address==0x600000||address==0x4863D0||address==0x4F7870||address==0x6A6300||address==0x7177C0);
 return R(1);
}
namespace featuretypes{enum{MagnetronDrop=1};}
static bool rawMagnetron(uint32_t p,int){return p==Actor;}
static bool objectAirQuery(uint32_t,int*out){*out=0;return true;}
static uint32_t readMapCell(int x,int y){return x>=0&&x<512&&y>=0&&y<512?Cell:0;}
#include "../source/type_select_bridge.h"
#include "../source/magnetron_drop_bridge.h"
#include LOCAL_PLAYER_ROUTES
static unsigned checks=0;
static void check(bool condition){++checks;if(!condition){fprintf(stderr,"check %u failed\n",checks);std::abort();}}
static void clearCounters(){reads=writes=nativeCalls=unselections=selections=resets=redraws=0;calls.clear();}
static void seed(){
 std::fill(memory.begin(),memory.end(),0);unavailable.clear();kinds.clear();worldObjects.clear();shared={};snap={};
 std::fill(std::begin(typePointers),std::end(typePointers),0);playerContext={};playerSidebarPending=false;playerContextChanged=false;playertest::SwitchAvailable=false;
 snap.typeCount=1;snap.types[0]={2,0,0};typePointers[0]=Type;
 for(uint32_t p:{Actor,Other}){kinds[p]=2;worldObjects.insert(p);put(p,0x610000u);put(p+0x21C,lastHouse);put(p+0x6C,100);put(p+0x6C4,Type);put(p+0x10,p);}
 put(0x610000+0x13C,0x600000u);put(0x887324,0x620000u);
 vectorAt(0xA8ECB8,0x630000,{Actor});vectorAt(0xA8EC78,0x640000,{Actor,Other});
 vectorAt(0xA80228,0x650000,{lastHouse,lastHouse+0x10000});
 for(unsigned n=0;n<2;++n){uint32_t house=lastHouse+n*0x10000;put(house+0x10,100u+n);put(house+0x34,0x660000u+n*0x1000);put(house+0x30,int(n));put<uint8_t>(house+0x1EC,n==0);}
 put(0xA83D4C,lastHouse);put(0xA8B230,0x670000u);put(0xA8ED84,100);put(0xA8E9A0,false);
 clearCounters();
}
int main(){
 uint8_t mask[MaxTypes]{};mask[0]=1;
 for(uint32_t broken:{0x48DC90u,0x6FBFA0u,0x6D2140u}){
  seed();unavailable.insert(broken);check(selectTypes(lastHouse,mask,false)==-12);check(!writes&&!nativeCalls&&!unselections&&!selections);
  check(selectVariant(lastHouse,false)==-12);check(!writes&&!nativeCalls&&!unselections&&!selections);
  unavailable.clear();check(selectTypes(lastHouse,mask,false)==2);check(unselections==1&&selections==2&&calls[0x6D2140]==2);
 }
 seed();unavailable.insert(0x6D2140);check(selectTypes(lastHouse,mask,true)==2);check(!calls[0x6D2140]&&unselections==1&&selections==2);
 seed();unavailable.insert(0x6D2140);check(selectVariant(lastHouse,true)==2);check(!calls[0x6D2140]&&unselections==1&&selections==2);
 seed();unavailable.insert(0x578080);check(selectTypes(lastHouse,mask,false)==2);check(unselections==1&&selections==2);
 // A beam-drop decision cannot infer a fabricated floor when height is unknown.
 seed();put(Actor+0x2AC,Other);put(Other+0x2B0,Actor);put(Other+0x9C,2560);put(Other+0xA0,2560);put(Other+0xA4,256);
 put(Cell+0xEC,0);put(Type+0x67C,0);put(Type+0x5B4,0);clearCounters();unavailable.insert(0x578080);
 check(!rawMagnetronDrop(Actor,Other,lastHouse));check(!reads&&!writes&&!nativeCalls);
 unavailable.clear();check(rawMagnetronDrop(Actor,Other,lastHouse));check(calls[0x578080]==1&&calls[0x4863D0]==1&&!writes);
 // The shipping player-switch feature stays disabled. The synthetic enabled
 // branch below only checks its latent transaction; it changes no product flag.
 seed();check(switchPlayerContext(lastHouse+0x10000,101)==-4);check(!writes&&!unselections&&!resets&&!playerContext.original);
 for(uint32_t broken:{0x48DC90u,0x6ABC60u,0x5F7900u,0x4F7870u,0x6A6300u}){
  seed();playertest::SwitchAvailable=true;unavailable.insert(broken);
  check(switchPlayerContext(lastHouse+0x10000,101)==-12);check(!writes&&!unselections&&!resets&&!playerContext.original&&!playerSidebarPending);
  unavailable.clear();check(switchPlayerContext(lastHouse+0x10000,101)==1);check(playerContext.original==lastHouse&&playerContext.applied==lastHouse+0x10000&&playerSidebarPending&&unselections==1&&resets==1);
  // Optional APIs disappearing must not prevent restoring owned House fields.
  unavailable.insert(broken);restorePlayerContext();check(!playerContext.original&&get<uint32_t>(0xA83D4C)==lastHouse&&get<uint8_t>(lastHouse+0x1EC)==1&&get<uint8_t>(lastHouse+0x10000+0x1EC)==0);
 }
 for(uint32_t broken:{0x6ABC60u,0x5F7900u,0x4F7870u,0x6A6300u}){
  seed();playerSidebarPending=true;unavailable.insert(broken);rebuildPlayerSidebar(lastHouse);
  check(playerSidebarPending&&!writes&&!nativeCalls&&!redraws);
  unavailable.clear();rebuildPlayerSidebar(lastHouse);check(!playerSidebarPending&&calls[0x6A6300]==1&&redraws==1&&get<uint8_t>(lastHouse+0x1FC)==1);
 }
 std::printf("{\"checks\":%u,\"passed\":true,\"real_windows_tested\":false,\"game_tested\":false}\n",checks);
}
