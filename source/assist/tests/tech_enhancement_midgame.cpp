// Source-extracted activation/cache regression. Native calls and hardware
// readiness are explicit models; no Windows/game process is opened.
#ifdef NDEBUG
#error This fixture requires assertions
#endif
#include <cassert>
#include <cstdint>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <sys/mman.h>
#include "tech_enhancement.h"
#include "tech_enhancement_parameter_policy.h"
#include "technology_mode_policy.h"
#include "sidebar_policy.h"
#include "session_epoch.h"
#include "catalogue_stamp.h"
using namespace assist;
using DWORD=uint32_t;using LONG=int32_t;
static constexpr int MaxTypes=512;
static constexpr uint32_t House=0x10000000,Bus=0x10020000,Normal=0x10022000,Factory=0x10030000,Product=0x10040000,Items=0x10050000;
static constexpr uint32_t Sidebar=0x87F7E8,Tabs=Sidebar+0x1544;
struct Type {char id[24];uint16_t name[40];int kind,index,naval,cost;};
struct Snapshot {int typeCount=0,techOpen=0;Type types[MaxTypes]{};} snap;
struct Settings {bool techEnhance=false;unsigned techEnhanceImplementation=0,techEnhanceCompareLevel=0;bool stop=false;int techEnhanceStatus=0;unsigned techEnhanceCount=0;} options;
static Settings*sh=&options;
static bool fault=false,enhancementChangedCatalogue=false,catalogueDirty=false,catalogueReady=false,techPending=false;
static uint64_t catalogueHash=0,techScanCatalogue=0;
static DWORD catalogueAt=0;
static int techScanAt=0,techScanVisible=0,techScanUnavailable=0;
static unsigned techImplementation=0;
static uint32_t typePointers[MaxTypes]{};
static uint8_t techAllowed[MaxTypes]{};
static sessionepoch::Tracker sessionTracker;
static int checks=0,additions=0,probes=0,factoryQueries=0,rechecks=0;
static bool hardwareReady=true,compatible=true,worldAvailable=true,factoryAvailable=true,ruleAllowed=true;
static uint32_t unreadable=0;
static bool mapped(uint32_t p,size_t n){return (p>=0x700000&&uint64_t(p)+n<=0xB00000)||(p>=0x10000000&&uint64_t(p)+n<=0x10100000);}
static bool read(uint32_t p,void*out,size_t n){if(!mapped(p,n)||(unreadable>=p&&uint64_t(unreadable)<uint64_t(p)+n))return false;std::memcpy(out,reinterpret_cast<void*>(uintptr_t(p)),n);return true;}
template<class T>static T get(uint32_t p){T v{};read(p,&v,sizeof v);return v;}
template<class T>static void put(uint32_t p,T v){assert(mapped(p,sizeof v));if(p==House+0x1FC)++rechecks;std::memcpy(reinterpret_cast<void*>(uintptr_t(p)),&v,sizeof v);}
static LONG InterlockedExchange(volatile LONG*p,LONG v){LONG old=*p;*p=v;return old;}
static bool enhancementWrite(uint32_t p,const void*v,size_t n){assert((p==Bus+0x634||p==Normal+0x634)&&n==4);std::memcpy(reinterpret_cast<void*>(uintptr_t(p)),v,n);return true;}
struct Vec {uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
static Vec vec(uint32_t p,int){Vec v{};return read(p,&v,sizeof v)?v:Vec{};}
static uint32_t item(Vec v,int n){return get<uint32_t>(v.items+n*4);}
static void name(uint32_t p,uint16_t*out,int n){for(int i=0;i<n;++i){out[i]=get<uint8_t>(p+0x24+i);if(!out[i])break;}}
static int kind(uint32_t){return 2;}
static uint32_t typeOf(uint32_t p,int){return get<uint32_t>(p+0x20);}
static bool nativeCanBuild(uint32_t type,bool limitOnly){return ruleAllowed&&(limitOnly||get<int>(type+0x634)>=0);}
template<class R,class...A>static R call(uint32_t address,uint32_t self,A...args){
 auto a=std::make_tuple(args...);
 if constexpr(sizeof...(A)==4){assert(address==0x5F7900);++factoryQueries;assert(std::get<0>(a)&&!std::get<1>(a)&&!std::get<2>(a));return R(factoryAvailable?Factory:0);}
 else if constexpr(sizeof...(A)==3){assert(address==0x4F7870&&self==House);return R(nativeCanBuild(uint32_t(uintptr_t(std::get<0>(a))),std::get<1>(a)));}
 else if constexpr(sizeof...(A)==2){
  assert(address==0x6A6300&&self==Sidebar);int k=int(std::get<0>(a)),index=int(std::get<1>(a));int tab=k==40?2:k==16?3:0;
  uint32_t strip=Tabs+tab*3988;int count=get<int>(strip+84);if(count>=75)return R(false);
  put<int>(strip+88+count*52,index);put<int>(strip+92+count*52,k);put<int>(strip+84,count+1);++additions;return R(true);
 }else{assert(false);return R{};}
}
template<class R>static R callFast(uint32_t address,int k,int,int){assert(address==0x6ABC60);return R(k==40?2:k==16?3:0);}
namespace techbp {static constexpr int Signature=-1,Thread=-5;}
namespace techparameters {
struct TypeIdentity {uint32_t pointer=0,id=0,table=0;};
struct EnhancedIdentity {TypeIdentity type;uint32_t items=0;int index=-1,level=0;};
struct Policy {volatile LONG enhanced=0,compareLevel=0;EnhancedIdentity enhancement[techenhancement::Count]{};} policy;
static bool enhancementDesired=false,enhancementCompareRequested=false,desired=false;
static int enhancementFailure=0;
static DWORD enhancementParameterScan=0;
static constexpr uint32_t FullGroup=1;
static bool ready(uint32_t){return hardwareReady;}
static bool fullType(uint32_t){return false;}
static void publish(){}
static bool enhancementCompatible(){return compatible;}
static bool enhancementReady(){return hardwareReady&&enhancementDesired;}
static bool captureWorld(uint32_t house){return worldAvailable&&house==House;}
static bool configure(){policy.enhanced=enhancementReady()?1:0;return hardwareReady;}
}
static bool techEnhanceParametersRetired(){return !techparameters::enhancementDesired;}
static bool restoreTechEnhanceParameters(){techparameters::policy.enhanced=0;techparameters::enhancementDesired=false;return true;}
static bool techDataBlocksEnhancement(){return (sh->techEnhance&&sh->techEnhanceImplementation==1)||!techEnhanceParametersRetired();}
static bool techDataPending(){return false;}
static bool nativeTextTechRetired(){return true;}
static int GuardedTechEnhancementProbe(unsigned house,unsigned type){assert(house==House);++probes;return techparameters::policy.enhanced&&ruleAllowed?1:0;}
static void deferTechEnhancementParameters();
static int typeSlot(uint32_t);
static int canonicalKind(int);
#include "tech_midgame_subject.inc"

static void check(bool ok,const char*what){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",what);std::exit(1);}}
static void mapMemory(){for(auto region:{std::pair<uint32_t,size_t>{0x700000,0x400000},{0x10000000,0x100000}}){void*p=mmap(reinterpret_cast<void*>(uintptr_t(region.first)),region.second,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(p==reinterpret_cast<void*>(uintptr_t(region.first)));}}
static void setup(){
 mapMemory();put<uint32_t>(0xA8B230,0x10060000);put<uint32_t>(0xA83D4C,House);put<int>(0xA8B238,3);put<int>(0xA8ED84,5000);
 put<uint32_t>(House+0x10,77);put<uint32_t>(House+0x53B4,Factory);put<uint32_t>(Factory+0x6C,House);put<uint32_t>(Factory+0x58,Product);put<uint32_t>(Product+0x20,Normal);
 put<uint32_t>(Factory+0x44,Items+0x100);put<int>(Factory+0x50,1);put<uint32_t>(Items+0x100,Normal);
 for(uint32_t p:{Bus,Normal}){put<uint32_t>(p,0x7F6218);put<uint32_t>(p+0x10,p>>8);put<uint32_t>(p+0x6CC,1);put<int>(p+0x610,500);}
 std::memcpy(reinterpret_cast<void*>(uintptr_t(Bus+0x24)),"BUS",4);std::memcpy(reinterpret_cast<void*>(uintptr_t(Normal+0x24)),"MTNK",5);
 put<int>(Bus+0x634,-1);put<int>(Normal+0x634,3);put<uint32_t>(0x7F6218+0x94,0x5F7900);
 put<uint32_t>(Items,Bus);put<uint32_t>(Items+4,Normal);put<Vec>(0xA83CE0,{0,Items,2,0,2,1});
 for(uint32_t p:{0xA8B218u,0xA8E348u,0xA83C68u})put<Vec>(p,{0,Items+0x200,0,0,0,1});
 sessionTracker.update(true,House,3,5000);scanTypes();external::catalogue::Stamp stamp;assert(external::catalogue::capture(read,stamp));catalogueHash=stamp.hash;catalogueReady=true;catalogueDirty=false;catalogueAt=1000;
 assert(typeSlot(Bus)<0&&typeSlot(Normal)>=0);techAllowed[typeSlot(Normal)]=1;
}
static std::vector<uint8_t> productionBytes(){std::vector<uint8_t> v(0x80+0x40+4);std::memcpy(v.data(),reinterpret_cast<void*>(uintptr_t(Factory)),0x80);std::memcpy(v.data()+0x80,reinterpret_cast<void*>(uintptr_t(Product)),0x40);std::memcpy(v.data()+0xC0,reinterpret_cast<void*>(uintptr_t(Items+0x100)),4);return v;}
static void exact(){options.techEnhance=true;options.techEnhanceImplementation=1;techEnhancementTick(1200);techEnhanceParametersTick(House,1200);}
static void ordinaryCataloguePoll(){external::catalogue::Stamp stamp;assert(external::catalogue::capture(read,stamp));if(!catalogueReady||catalogueHash!=stamp.hash)scanTypes();catalogueHash=stamp.hash;catalogueReady=true;catalogueDirty=false;}
int main(int argc,char**argv){assert(argc==2);setup();const std::string test=argv[1];const auto queue=productionBytes();
 if(test=="legacy_midgame_refresh"){
  options.techEnhance=true;techEnhancementTick(1200);
  check(get<int>(Bus+0x634)==1&&options.techEnhanceStatus==1,"legacy level is applied in an already-running match");
  check(typeSlot(Bus)>=0,"legacy changed level enters the host catalogue");
  check(get<uint8_t>(House+0x1FC)==1,"legacy activation requests native House tech-tree evaluation");
  check(!additions&&!probes,"legacy delegates native sidebar enumeration without direct cameo injection");
 }else if(test=="legacy_recheck_once"){
  options.techEnhance=true;techEnhancementTick(1200);check(rechecks==1,"first changed level requests one recheck");
  put<uint8_t>(House+0x1FC,0);rechecks=0;techEnhancementTick(1800);techEnhancementTick(2400);
  check(!rechecks,"unchanged recurring scans do not repeatedly rebuild native options");
  options.techEnhance=false;techEnhancementTick(2500);check(get<int>(Bus+0x634)==-1,"turning off retains the original level restoration");
 }else if(test=="legacy_inactive_master"){
  techImplementation=2;options.techEnhance=true;techEnhancementTick(1200);check(rechecks==1,"inactive selected Master does not suppress independent enhancement refresh");
 }else if(test=="legacy_active_master"||test=="legacy_pending_master"){
  techImplementation=2;snap.techOpen=test=="legacy_active_master"?3:0;techPending=test=="legacy_pending_master";
  options.techEnhance=true;techEnhancementTick(1200);check(get<int>(Bus+0x634)==1,"independent legacy enhancement still applies");
  check(!rechecks&&!additions,"active or pending Master retains its documented native-trigger-only sidebar policy");
 }else if(test=="legacy_no_change"){
  put<int>(Bus+0x634,3);options.techEnhance=true;techEnhancementTick(1200);check(!rechecks&&get<int>(Bus+0x634)==3,"ineligible native level does not request a sidebar rebuild");
 }else if(test=="legacy_unreadable_world"){
  unreadable=0xA8B230;options.techEnhance=true;techEnhancementTick(1200);check(!rechecks&&get<int>(Bus+0x634)==-1,"failed world admission does not write levels or the native refresh flag");
 }else if(test=="exact_midgame_catalogue"){
  const auto originalStamp=catalogueHash;exact();ordinaryCataloguePoll();techEnhanceParametersTick(House,1800);
  check(catalogueHash==originalStamp&&get<int>(Bus+0x634)==-1,"exact activation leaves the native catalogue stamp and original level unchanged");
  check(techparameters::policy.enhancement[10].type.pointer==Bus&&techparameters::policy.enhanced,"exact policy recognizes BUS and is ready");
  check(typeSlot(Bus)>=0,"exact activation invalidates the formerly filtered catalogue");
  check(additions==1&&probes==1&&factoryQueries==1,"newly eligible cameo reaches existing probe/factory/add path once");
  check(techAllowed[typeSlot(Normal)]==1&&!techAllowed[typeSlot(Bus)],"catalogue rebuild preserves old full-tech selection by pointer without selecting the new type");
  check(!rechecks,"exact mode does not invoke the unmodified native building enumeration");
 }else if(test=="exact_pending_readiness"){
  hardwareReady=false;exact();check(!probes&&!additions,"not-ready hardware does no cameo work");
  ordinaryCataloguePoll();hardwareReady=true;techEnhanceParametersTick(House,1800);
  check(typeSlot(Bus)>=0&&additions==1,"later readiness uses the refreshed catalogue");
  catalogueReady=true;catalogueDirty=false;techEnhanceParametersTick(House,2400);check(catalogueReady&&!catalogueDirty&&additions==1,"unchanged request does not keep invalidating or duplicating icons");
 }else if(test=="exact_off_reenable"){
  exact();check(typeSlot(Bus)>=0&&additions==1,"first midgame activation succeeds");
  options.techEnhance=false;techEnhanceParametersTick(House,1800);check(typeSlot(Bus)<0&&!techparameters::policy.enhanced,"off edge removes only effective catalogue permission");
  put<int>(Tabs+2*3988+84,0);options.techEnhance=true;techEnhanceParametersTick(House,2400);check(typeSlot(Bus)>=0&&additions==2,"later reenable repopulates a natively pruned cameo");
 }else if(test=="exact_native_rule_rejected"||test=="exact_factory_missing"){
  ruleAllowed=test!="exact_native_rule_rejected";factoryAvailable=test!="exact_factory_missing";exact();
  check(typeSlot(Bus)>=0&&probes==1&&!additions,"catalogue repair never bypasses the existing native rule/factory gate");
 }else if(test=="exact_filter_insignificant"||test=="exact_filter_spawned"||test=="exact_filter_no_owner"){
  if(test=="exact_filter_insignificant")put<uint8_t>(Bus+0x232,1);
  else if(test=="exact_filter_spawned")put<uint8_t>(Bus+0xD54,1);else put<uint32_t>(Bus+0x6CC,0);
  exact();check(typeSlot(Bus)<0&&!probes&&!additions,"effective level preserves insignificant/spawned/owner catalogue filters");
 }else if(test=="exact_full_capacity"){
  put<int>(Tabs+2*3988+84,75);for(int i=0;i<75;++i){put<int>(Tabs+2*3988+88+i*52,i+100);put<int>(Tabs+2*3988+92+i*52,40);}
  exact();check(probes==1&&factoryQueries==1&&!additions,"native strip capacity stays at seventy-five");
 }else if(test=="exact_failed_signature"){
  compatible=false;exact();check(!probes&&!additions&&!techparameters::policy.enhanced,"invalid signature never publishes effective engine permission");
 }else if(test=="exact_full_allowlist"){
  snap.techOpen=5;techImplementation=4;exact();check(techPending&&typeSlot(Bus)>=0,"exact catalogue edge requests the existing active full-tech refresh");
  check(techAllowed[typeSlot(Normal)]==1&&!techAllowed[typeSlot(Bus)],"full-tech whitelist remains explicit and preserves existing selection");
 }else if(test=="exact_busy_mode_handoff"){
  exact();put<uint32_t>(Product+0x20,Bus);options.techEnhanceImplementation=0;techEnhanceParametersTick(House,1800);
  check(techparameters::enhancementFailure==-7&&techparameters::policy.enhanced,"dependent in-flight product keeps exact permission during a requested mode switch");
  put<uint32_t>(Product+0x20,Normal);
 }else{std::fprintf(stderr,"unknown case\n");return 2;}
 check(productionBytes()==queue,"activation does not alter the in-flight factory/product/queue bytes");
 std::printf("{\"case\":\"%s\",\"status\":\"PASS\",\"checks\":%d}\n",test.c_str(),checks);return 0;
}
