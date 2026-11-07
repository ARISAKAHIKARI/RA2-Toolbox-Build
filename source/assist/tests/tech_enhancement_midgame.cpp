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
static int checks=0,additions=0,probes=0,factoryQueries=0,tabQueries=0,rechecks=0,levelWrites=0;
static bool hardwareReady=true,compatible=true,bodyBypassed=false,worldAvailable=true,factoryAvailable=true,ruleAllowed=true;
// Named-rule fixtures use synthetic owner groups, not game country bit indices.
// The native predicate itself remains modeled; these gates must stay effective
// after a civilian is admitted to the tool's catalogue.
static constexpr uint32_t ModelAlliedOwners=1,ModelSovietOwners=2,ModelYuriOwners=4;
static bool civilianRules=false,modelPrerequisiteOwned=true;
static uint32_t modelFactoryOwners=ModelSovietOwners;
static uint32_t unreadable=0,blockedDependency=0;
static bool nativeDependencyReady(uint32_t address){return address!=blockedDependency;}
template<class...A>static bool nativeDependenciesReady(A...addresses){return (nativeDependencyReady(uint32_t(addresses))&&...);}
static bool mapped(uint32_t p,size_t n){return (p>=0x700000&&uint64_t(p)+n<=0xB00000)||(p>=0x10000000&&uint64_t(p)+n<=0x10100000);}
static bool read(uint32_t p,void*out,size_t n){if(!mapped(p,n)||(unreadable>=p&&uint64_t(unreadable)<uint64_t(p)+n))return false;std::memcpy(out,reinterpret_cast<void*>(uintptr_t(p)),n);return true;}
template<class T>static T get(uint32_t p){T v{};read(p,&v,sizeof v);return v;}
template<class T>static void put(uint32_t p,T v){assert(mapped(p,sizeof v));if(p==House+0x1FC)++rechecks;std::memcpy(reinterpret_cast<void*>(uintptr_t(p)),&v,sizeof v);}
static LONG InterlockedExchange(volatile LONG*p,LONG v){LONG old=*p;*p=v;return old;}
static bool enhancementWrite(uint32_t p,const void*v,size_t n){assert((p==Bus+0x634||p==Normal+0x634)&&n==4);++levelWrites;std::memcpy(reinterpret_cast<void*>(uintptr_t(p)),v,n);return true;}
struct Vec {uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
static Vec vec(uint32_t p,int){Vec v{};return read(p,&v,sizeof v)?v:Vec{};}
static uint32_t item(Vec v,int n){return get<uint32_t>(v.items+n*4);}
static void name(uint32_t p,uint16_t*out,int n){for(int i=0;i<n;++i){out[i]=get<uint8_t>(p+0x24+i);if(!out[i])break;}}
static int kind(uint32_t){return 2;}
static uint32_t typeOf(uint32_t p,int){return get<uint32_t>(p+0x20);}
static bool nativeNonLevelRules(uint32_t type){return ruleAllowed&&(!civilianRules||get<int>(type+0x648)==0||modelPrerequisiteOwned);}
static bool nativeCanBuild(uint32_t type,bool limitOnly){return ruleAllowed&&(limitOnly||(nativeNonLevelRules(type)&&get<int>(type+0x634)>=0));}
template<class R,class...A>static R call(uint32_t address,uint32_t self,A...args){
 assert(address!=blockedDependency);
 auto a=std::make_tuple(args...);
 if constexpr(sizeof...(A)==4){assert(address==0x5F7900);++factoryQueries;assert(std::get<0>(a)&&!std::get<1>(a)&&!std::get<2>(a));return R(factoryAvailable&&(!civilianRules||(get<uint32_t>(self+0x6CC)&modelFactoryOwners))?Factory:0);}
 else if constexpr(sizeof...(A)==3){assert(address==0x4F7870&&self==House);return R(nativeCanBuild(uint32_t(uintptr_t(std::get<0>(a))),std::get<1>(a)));}
 else if constexpr(sizeof...(A)==2){
  assert(address==0x6A6300&&self==Sidebar);int k=int(std::get<0>(a)),index=int(std::get<1>(a));int tab=k==40?2:k==16?3:0;
  uint32_t strip=Tabs+tab*3988;int count=get<int>(strip+84);if(count>=75)return R(false);
  put<int>(strip+88+count*52,index);put<int>(strip+92+count*52,k);put<int>(strip+84,count+1);++additions;return R(true);
 }else{assert(false);return R{};}
}
template<class R>static R callFast(uint32_t address,int k,int,int){assert(address==0x6ABC60&&address!=blockedDependency);++tabQueries;return R(k==40?2:k==16?3:0);}
namespace techbp {static constexpr int Signature=-1,Thread=-5;}
namespace techparameters {
struct TypeIdentity {uint32_t pointer=0,id=0,table=0;};
struct EnhancedIdentity {TypeIdentity type;uint32_t items=0;int index=-1,level=0;};
struct Policy {volatile LONG enhanced=0,compareLevel=0;EnhancedIdentity enhancement[techenhancement::Count]{};} policy;
static bool enhancementDesired=false,enhancementCompareRequested=false,desired=false,enhancementCameoBlocked=false;
static int enhancementFailure=0;
static DWORD enhancementParameterScan=0;
static constexpr uint32_t FullGroup=1;
static bool ready(uint32_t){return hardwareReady;}
static bool fullType(uint32_t){return false;}
static void publish(){}
static bool enhancementCompatible(){return compatible;}
static bool enhancementBodyBypassed(){return bodyBypassed;}
static bool enhancementReady(){return hardwareReady&&enhancementDesired;}
static bool captureWorld(uint32_t house){return worldAvailable&&house==House;}
static bool configure(){policy.enhanced=enhancementReady()?1:0;return hardwareReady;}
}
static bool techEnhanceParametersRetired(){return !techparameters::enhancementDesired;}
static bool restoreTechEnhanceParameters(){techparameters::policy.enhanced=0;techparameters::enhancementDesired=false;techparameters::enhancementCameoBlocked=false;return true;}
static bool techDataBlocksEnhancement(){return (sh->techEnhance&&sh->techEnhanceImplementation==1)||!techEnhanceParametersRetired();}
static bool techDataPending(){return false;}
static bool nativeTextTechRetired(){return true;}
static int GuardedTechEnhancementProbe(unsigned house,unsigned type){assert(house==House&&blockedDependency!=0x4F7870);++probes;return techparameters::policy.enhanced&&nativeNonLevelRules(type)?1:0;}
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
static void civilian(bool president){
 // Public original-rule candidates agree on these distinctions: PRES and BUS
 // are Insignificant; PRES has all owners and no prerequisite, while BUS has
 // Soviet/Yuri owners and NAWEAP. This is not a snapshot of the user's MOD.
 civilianRules=true;put<uint8_t>(Bus+0x232,1);
 put<uint32_t>(Bus+0x6CC,president?ModelAlliedOwners|ModelSovietOwners|ModelYuriOwners:ModelSovietOwners|ModelYuriOwners);
 if(president){
  put<uint32_t>(Bus,0x7EB610);std::memcpy(reinterpret_cast<void*>(uintptr_t(Bus+0x24)),"PRES",5);put<uint32_t>(0x7EB610+0x94,0x5F7900);
  put<uint32_t>(Items,Normal);put<Vec>(0xA83CE0,{0,Items,1,0,1,1});
  put<uint32_t>(Items+0x300,Bus);put<Vec>(0xA8E348,{0,Items+0x300,1,0,1,1});
 }else{
  // Index 0 represents the synthetic NAWEAP prerequisite, not its game index.
  put<uint32_t>(Items+0x400,0);put<Vec>(Bus+0x638,{0,Items+0x400,1,0,1,1});
 }
}
static std::vector<uint8_t> civilianRuleBytes(){
 std::vector<uint8_t> bytes(0x2000);std::memcpy(bytes.data(),reinterpret_cast<void*>(uintptr_t(Bus)),bytes.size());
 // Only the existing owned TechLevel write is allowed in legacy mode.
 std::memset(bytes.data()+0x634,0,4);return bytes;
}
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
 }else if(test=="legacy_civilian_bus"||test=="legacy_civilian_pres"){
  const bool president=test=="legacy_civilian_pres";civilian(president);const auto rules=civilianRuleBytes();
  options.techEnhance=true;techEnhancementTick(1200);
  check(get<int>(Bus+0x634)==1&&options.techEnhanceStatus==1,"legacy civilian level and status are applied");
  check(typeSlot(Bus)>=0,president?"legacy enhanced PRES with Insignificant enters the catalogue":"legacy enhanced BUS with Insignificant enters the catalogue");
  check(rechecks==1&&!probes&&!factoryQueries&&!additions,"legacy civilian still requests native enumeration without direct cameo insertion");
  check(civilianRuleBytes()==rules,"legacy civilian admission changes no other rule byte");
  options.techEnhance=false;techEnhancementTick(1800);scanTypes();
  check(get<int>(Bus+0x634)==-1&&typeSlot(Bus)<0,"legacy civilian off restores its level and removes catalogue admission");
  check(civilianRuleBytes()==rules,"legacy civilian restoration leaves Insignificant Owner and prerequisite intact");
 }else if(test=="exact_civilian_bus"||test=="exact_civilian_pres"){
  const bool president=test=="exact_civilian_pres";civilian(president);const auto rules=civilianRuleBytes();exact();
  check(techparameters::policy.enhanced&&techparameters::policy.enhancement[president?3:10].type.pointer==Bus,"exact policy captures the named civilian identity");
  check(typeSlot(Bus)>=0,president?"exact enhanced PRES with Insignificant enters the catalogue":"exact enhanced BUS with Insignificant enters the catalogue");
  check(probes==1&&factoryQueries==1&&additions==1,"admitted civilian still passes the native probe and factory before cameo insertion");
  check(get<int>(Bus+0x634)==-1&&civilianRuleBytes()==rules,"exact civilian admission keeps every original rule byte");
  options.techEnhance=false;techEnhanceParametersTick(House,1800);
  check(typeSlot(Bus)<0&&!techparameters::policy.enhanced,"exact civilian off removes its temporary catalogue permission");
  check(get<int>(Bus+0x634)==-1&&civilianRuleBytes()==rules,"exact civilian off retains every original rule byte");
 }else if(test=="exact_civilian_inactive"||test=="exact_civilian_not_whitelisted"||test=="exact_civilian_wrong_vtable"||test=="exact_civilian_wrong_array"||test=="exact_civilian_unreadable"||test=="exact_civilian_ineligible_level"){
  civilian(false);const bool inactive=test=="exact_civilian_inactive";
  if(inactive||test=="exact_civilian_not_whitelisted"||test=="exact_civilian_ineligible_level")put<int>(Bus+0x634,1);
  if(test=="exact_civilian_not_whitelisted")std::memcpy(reinterpret_cast<void*>(uintptr_t(Bus+0x24)),"TAXI",5);
  if(test=="exact_civilian_wrong_vtable")put<uint32_t>(Bus,0x7EB610);
  if(test=="exact_civilian_wrong_array"){
   put<uint32_t>(Items,Normal);put<Vec>(0xA83CE0,{0,Items,1,0,1,1});
   put<uint32_t>(Items+0x300,Bus);put<Vec>(0xA8E348,{0,Items+0x300,1,0,1,1});
  }
  if(test=="exact_civilian_unreadable")unreadable=Bus+0x24;
  if(inactive)scanTypes();else exact();
  check(typeSlot(Bus)<0&&!probes&&!factoryQueries&&!additions,"civilian exception requires active eligible whitelist identity and readable membership");
  check(get<uint8_t>(Bus+0x232)==1,"refused civilian retains its Insignificant flag");
 }else if(test=="legacy_civilian_unowned"||test=="legacy_civilian_conflict"||test=="legacy_civilian_wrong_index"||test=="legacy_civilian_world_changed"||test=="legacy_civilian_world_unreadable"||test=="legacy_civilian_stopped"||test=="legacy_civilian_fault"){
  civilian(false);options.techEnhance=true;
  if(test=="legacy_civilian_unowned")put<int>(Bus+0x634,1);else techEnhancementTick(1200);
  if(test=="legacy_civilian_conflict")enhancedTypes[10].conflict=true;
  if(test=="legacy_civilian_wrong_index")enhancedTypes[10].index=1;
  if(test=="legacy_civilian_world_changed")put<uint32_t>(0xA8B230,0x10061000);
  if(test=="legacy_civilian_world_unreadable")unreadable=0xA8B230;
  if(test=="legacy_civilian_stopped")options.stop=true;
  if(test=="legacy_civilian_fault")fault=true;
  scanTypes();check(typeSlot(Bus)<0,"legacy civilian exception requires current conflict-free owned identity and an active request");
  check(get<uint8_t>(Bus+0x232)==1,"legacy admission refusal never changes Insignificant");
 }else if(test=="exact_civilian_missing_prerequisite"||test=="exact_civilian_factory_owner_mismatch"){
  civilian(false);const auto rules=civilianRuleBytes();
  if(test=="exact_civilian_missing_prerequisite")modelPrerequisiteOwned=false;else modelFactoryOwners=ModelAlliedOwners;
  exact();check(typeSlot(Bus)>=0&&probes==1&&!additions,"civilian catalogue admission does not bypass prerequisite or factory owner gates");
  check(factoryQueries==(modelPrerequisiteOwned?1:0),"prerequisite rejection precedes the factory query");
  check(civilianRuleBytes()==rules&&get<int>(Bus+0x634)==-1,"rejected production retains the named civilian rules");
 }else if(test=="exact_native_rule_rejected"||test=="exact_factory_missing"){
  civilian(false);
  ruleAllowed=test!="exact_native_rule_rejected";factoryAvailable=test!="exact_factory_missing";exact();
  check(typeSlot(Bus)>=0&&probes==1&&!additions,"catalogue repair never bypasses the existing native rule/factory gate");
 }else if(test=="exact_filter_spawned"||test=="exact_filter_no_owner"){
  civilian(false);if(test=="exact_filter_spawned")put<uint8_t>(Bus+0xD54,1);else put<uint32_t>(Bus+0x6CC,0);
  exact();check(typeSlot(Bus)<0&&!probes&&!additions,"civilian exception preserves spawned and owner catalogue filters");
 }else if(test=="exact_full_capacity"){
  put<int>(Tabs+2*3988+84,75);for(int i=0;i<75;++i){put<int>(Tabs+2*3988+88+i*52,i+100);put<int>(Tabs+2*3988+92+i*52,40);}
  exact();check(probes==1&&factoryQueries==1&&!additions,"native strip capacity stays at seventy-five");
 }else if(test=="exact_failed_signature"){
  compatible=false;exact();check(!probes&&!additions&&!techparameters::policy.enhanced,"invalid signature never publishes effective engine permission");
 }else if(test=="exact_dependency_canbuild"||test=="exact_dependency_factory"||test=="exact_dependency_tab"||test=="exact_dependency_addcameo"){
  blockedDependency=test=="exact_dependency_canbuild"?0x4F7870:test=="exact_dependency_factory"?0x5F7900:test=="exact_dependency_tab"?0x6ABC60:0x6A6300;
  put<int>(Tabs+2*3988+84,1);put<int>(Tabs+2*3988+88,1);put<int>(Tabs+2*3988+92,40);
  const std::vector<uint8_t> strip(reinterpret_cast<uint8_t*>(uintptr_t(Tabs+2*3988)),reinterpret_cast<uint8_t*>(uintptr_t(Tabs+3*3988)));
  exact();check(techparameters::enhancementCameoBlocked&&!probes&&!factoryQueries&&!tabQueries&&!additions,"missing helper blocks the whole presentation pass before native calls");
  check(!std::memcmp(strip.data(),reinterpret_cast<void*>(uintptr_t(Tabs+2*3988)),strip.size()),"missing helper preserves every existing sidebar byte");
  check(techparameters::policy.enhanced&&!techparameters::enhancementFailure&&!levelWrites,"presentation refusal retains verified Exact permission without rule writes");
  blockedDependency=0;techEnhanceParametersTick(House,1800);
  check(!techparameters::enhancementCameoBlocked&&additions==1&&get<int>(Tabs+2*3988+84)==2,"recovered helper retries locally and preserves the existing icon");
 }else if(test=="exact_dependency_active_handoff"){
  exact();put<uint32_t>(Product+0x20,Bus);blockedDependency=0x6A6300;techEnhanceParametersTick(House,1800);
  check(techparameters::enhancementCameoBlocked&&techparameters::policy.enhanced&&!techparameters::enhancementFailure,"late sidebar failure preserves an in-flight Exact product");
  blockedDependency=0x4F7870;options.techEnhanceImplementation=0;techEnhanceParametersTick(House,2400);
  check(techparameters::enhancementFailure==-7&&techparameters::policy.enhanced,"unavailable CanBuild cannot prove the old permission safe to retire");
  blockedDependency=0;put<uint32_t>(Product+0x20,Normal);techEnhanceParametersTick(House,3000);
  check(!techparameters::policy.enhanced&&!techparameters::enhancementCameoBlocked,"confirmed idle handoff retires Exact and clears presentation failure");
 }else if(test=="production_dependency_canbuild"){
  blockedDependency=0x4F7870;check(techEnhancementProductionPending(House),"missing CanBuild keeps production retirement pending");
  blockedDependency=0;check(!techEnhancementProductionPending(House),"recovered CanBuild can establish native production independence");
 }else if(test=="exact_ares_body_bypassed"){
  bodyBypassed=true;exact();
  check(techparameters::enhancementFailure==techenhanceparams::NativeBodyBypassed&&!probes&&!additions&&!techparameters::policy.enhanced,"Ares replacement reports only the unavailable Exact comparison path");
  check(options.techEnhanceImplementation==1&&!levelWrites&&get<int>(Bus+0x634)==-1,"Exact refusal preserves the selected mode and never falls back to rule writes");
  bodyBypassed=false;techEnhanceParametersTick(House,1800);
  check(!techparameters::enhancementFailure&&techparameters::policy.enhanced&&additions==1,"a restored native body can retry locally without restarting the bridge");
 }else if(test=="rules_to_exact_ares_busy_handoff"){
  options.techEnhance=true;techEnhancementTick(1200);check(levelWrites==1&&get<int>(Bus+0x634)==1,"Rules owns the original hidden level before the mode switch");
  put<uint32_t>(Product+0x20,Bus);options.techEnhanceImplementation=1;bodyBypassed=true;
  techEnhancementTick(1400);techEnhanceParametersTick(House,1400);
  check(techparameters::enhancementFailure==-7&&enhancedTypes[10].owned&&levelWrites==1&&get<int>(Bus+0x634)==1,"in-flight Rules product is preserved while the unavailable Exact mode waits");
  put<uint32_t>(Product+0x20,Normal);techEnhancementTick(1800);techEnhanceParametersTick(House,1800);
  check(!enhancedTypes[10].owned&&levelWrites==2&&get<int>(Bus+0x634)==-1,"completed handoff restores exactly the original rule value");
  check(techparameters::enhancementFailure==techenhanceparams::NativeBodyBypassed&&!probes&&!additions,"Ares local status is published after production-safe Rules retirement");
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
