// Actual production functions/branches run against a bounded memory/API model.
#ifdef NDEBUG
#error Assertions must remain enabled
#endif
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <vector>
#include "../source/signatures.h"
#include "../source/startup_dependency_policy.h"
#include "../source/startup_signature_compat.h"
#include "../../shared/source/direct_patch_policy.h"
using DWORD=uint32_t;

static unsigned checks=0;
static void check(bool okay){++checks;if(!okay){std::fprintf(stderr,"startup dependency check %u failed\n",checks);std::abort();}}
static std::map<uint32_t,uint8_t> memory;
static std::vector<uint32_t> reads;
static bool read(uint32_t at,void*out,size_t size){
 reads.push_back(at);for(size_t i=0;i<size;++i){auto p=memory.find(at+uint32_t(i));if(p==memory.end())return false;static_cast<uint8_t*>(out)[i]=p->second;}return true;
}
static void put(uint32_t at,const void*data,size_t size){const auto*p=static_cast<const uint8_t*>(data);for(size_t i=0;i<size;++i)memory[at+uint32_t(i)]=p[i];}
template<class T>static void put(uint32_t at,T value){put(at,&value,sizeof value);}
template<class T>static T get(uint32_t at){T value{};read(at,&value,sizeof value);return value;}
// The native/unknown entry cases finish before these Win32 boundary substitutes.
// This fixture does not invent an accepted MRGA function body.
struct IMAGE_DOS_HEADER {uint16_t e_magic=0;int32_t e_lfanew=0;};
struct IMAGE_NT_HEADERS32 {uint32_t Signature=0;struct {uint16_t Machine=0;}FileHeader;struct {uint16_t Magic=0;uint32_t SizeOfImage=0;}OptionalHeader;};
struct MEMORY_BASIC_INFORMATION {void*BaseAddress=nullptr,*AllocationBase=nullptr;size_t RegionSize=0;unsigned State=0,Type=0,Protect=0;};
constexpr unsigned IMAGE_DOS_SIGNATURE=0x5a4d,IMAGE_NT_SIGNATURE=0x4550,IMAGE_FILE_MACHINE_I386=0x14c,IMAGE_NT_OPTIONAL_HDR32_MAGIC=0x10b;
constexpr unsigned MEM_COMMIT=0x1000,MEM_IMAGE=0x1000000,MEM_PRIVATE=0x20000,PAGE_NOACCESS=1,PAGE_GUARD=0x100,PAGE_EXECUTE=0x10,PAGE_EXECUTE_READ=0x20,PAGE_EXECUTE_READWRITE=0x40,PAGE_EXECUTE_WRITECOPY=0x80;
static void*GetModuleHandleW(const wchar_t*){return nullptr;}
static size_t VirtualQuery(void*,MEMORY_BASIC_INFORMATION*,size_t){return 0;}
static uint32_t clockMs=100;
static uint32_t GetTickCount();
#include "../source/native_entry_runtime.h"
#include STARTUP_HELPERS

static unsigned nativeCalls=0,crcBegins=0,crcEnds=0;
static uint32_t lastNative=0;
template<class T,class... Args>static T call(uint32_t address,Args...){++nativeCalls;lastNative=address;}
static bool crcMapToolBegin(uint32_t){++crcBegins;return true;}
static void crcMapToolEnd(){++crcEnds;}
enum {ClearBeacons=7,RevealMap=8};
constexpr unsigned ExperimentalReveal=1,BuildAnywhere=2,NativeAirWaypoints=4;
struct Shared {
 unsigned flags=ExperimentalReveal,renderReveal=0;
 unsigned sidebarMode=0,moFeatures=0,nativeSellCursor=0,nativeMegaGateway=0,
 controlCommandOwner=0,controlAll=0,networkHousePreserve=0,buildTimeTooltip=0,
 crcMapShadow=0,techEnhance=0,lowDamageFilter=0,lowDamageNetwork=0,
 scrollRefreshMode=0,scrollRefreshHz=0;
}storage,*sh=&storage;
struct Snapshot {int status=1,commandStatus=0,renderRevealStatus=0;}snap;
static bool checked=false,good=false,coreReached=false;
static uint32_t signatureRetryAt=0;
static unsigned published=0;
static void publishSnapshot(uint32_t){++published;}
static void activation(uint32_t now){
#include STARTUP_GATE
 coreReached=true;
}
struct CommandResult {int result,count;};
static CommandResult command(int c){int result=1,n=0;uint32_t house=0x10000;
 if(false){}
#include OPTIONAL_COMMANDS
 return {result,n};
}

// Registry ownership is a substitute; both group traversal functions below
// are production code, as are their site lists and expected bytes.
struct Registry {
 std::set<uint32_t> owned,foreign;
 bool active(uint32_t at)const{return owned.count(at);}
 bool conflict(uint32_t at)const{return foreign.count(at);}
}codeHookRegistry;
static unsigned patchPreflights=0,patchAttempts=0,patchInstalls=0,patchRestores=0;
namespace patchcontext {
template<class Read>static bool compatible(uint32_t at,Read reader){++patchPreflights;auto site=directpatch::find(at);return site&&directpatch::compatible(*site,reader);}
}
namespace gameentry {
enum class Client {Assist};
static bool patch(Client,uint32_t at,const uint8_t*before,const uint8_t*after,unsigned size){
 ++patchAttempts;uint8_t actual[16]{};
 if(codeHookRegistry.conflict(at)||!read(at,actual,size)||std::memcmp(actual,before,size))return false;
 auto site=directpatch::find(at);if(!site)return false;
 const bool restore=codeHookRegistry.active(at);
 if(restore){if(std::memcmp(before,site->after,size)||std::memcmp(after,site->before,size))return false;codeHookRegistry.owned.erase(at);++patchRestores;}
 else {if(std::memcmp(before,site->before,size)||std::memcmp(after,site->after,size))return false;codeHookRegistry.owned.insert(at);++patchInstalls;}
 put(at,after,size);return true;
}
}
#include "../source/direct_modes_bridge.h"

namespace shroudpolicy {static bool materialMode(unsigned mode){return mode==4||mode==5;}}
static bool writeShroudMaterial(uint32_t,const void*,size_t){return true;}
struct Materials {
 template<class Read,class Write>bool restore(Read,Write){return true;}
 bool changed()const{return false;}
 template<class Read,class Write>int update(int,uint32_t,Read,Write){return 1;}
}shroudMaterials;
static bool riskRenderActive=false,riskRenderRedrawPending=false;
static unsigned riskSites[10]{};
static void prepareRiskSites(){}
static bool restoreRiskSite(unsigned){return true;}
static bool installRiskSite(unsigned){return true;}
static uint32_t GetTickCount(){return clockMs;}
static bool redrawRevealTest(){return false;}
#include RENDER_REVEAL
namespace pauseview {constexpr unsigned Feature=1;}
namespace controlmode {static bool native(unsigned){return false;}static bool sender(unsigned){return false;}}
static bool nativeViewNeedsTracking(){return false;}
static bool riskPlacementActive=false,riskAirActive=false,riskControlActive=false,riskLowDamageActive=false;
static void restoreMapVisibilityParameters(){}static void restoreSidebarView(){}
static void restorePauseView(){}static void restoreNativeSellCursor(){}
static void restoreNativeMegaGateway(){}static void restoreControlObjectOwner(){}
static void restoreControlCommandOwner(){}static void controlSitesSuspend(){}
static void restoreNetworkSender(){}static void restoreBuildTimeTooltip(){}
static void restoreCrcMapShadow(){}static void restoreTechEnhancement(){}
static void restoreHighScroll(){}
#include SUSPENDED_OPTIONS

static void seed(){
 memory.clear();reads.clear();storage={};snap={};checked=good=coreReached=false;signatureRetryAt=0;published=0;clockMs=100;
 nativeAresCache={};for(auto&cache:nativeEntryCache)cache={};for(auto&cache:nativeDependencyCache)cache={};
 nativeCalls=crcBegins=crcEnds=lastNative=0;patchPreflights=patchAttempts=patchInstalls=patchRestores=0;codeHookRegistry={};riskRenderActive=riskRenderRedrawPending=false;
 for(const auto&s:Signatures)put(s.address,s.bytes,s.length);
 for(const auto&s:directpatch::Sites){put(s.address,s.before,s.size);put(s.address+s.size,s.context,sizeof s.context);}
 put<uint32_t>(0x87F7E8+316,0x20000);put<int>(0x87F7E8+320,1024);put<int>(0x10000+0x30,0);
}
static void unknown(uint32_t at){const uint8_t jump[]={0xe9,0x11,0x22,0x33,0x44};put(at,jump,sizeof jump);}
static void expectNoNative(){check(!nativeCalls&&!crcBegins&&!crcEnds);}
static void dependencyChecks(){
 seed();check(startupdependency::dependency(0x431410)==startupdependency::Dependency::ClearBeacons);check(startupdependency::dependency(0x577D90)==startupdependency::Dependency::Reveal);
 check(startupdependency::dependency(0x6AA600)==startupdependency::Dependency::Unused);
 unsigned count=0;for(const auto&s:Signatures)count+=startupdependency::requiredAtStartup(s.address);check(count==0);
 // Every known API belongs to its consumer, including shared constructor and
 // selection helpers. Their failure cannot prevent unrelated Runtime work.
 for(const auto&s:Signatures)for(unsigned inaccessible=0;inaccessible<2;++inaccessible){
  seed();if(inaccessible)memory.erase(s.address);else memory[s.address]^=1;const auto before=memory;
  reads.clear();check(signatures()&&reads.empty());activation(100);
  check(coreReached&&good&&checked&&!published&&snap.status==1&&snap.commandStatus==0);
  check(!nativeDependencyReady(s.address,true));check(memory==before);
 }
 // Classification and admission of an unreviewed/new address remain closed.
 seed();const Signature future[]={{0x123450,1,{0x90}}};
 check(startupdependency::requiredAtStartup(future[0].address));
 check(!startupdependency::coreReady(future,[](const Signature&s){return startupSignature(s);}));
 put<uint8_t>(future[0].address,0x90);check(!nativeDependencyReady(future[0].address));
 seed();unknown(0x431410);unknown(0x577D90);reads.clear();check(signatures()&&reads.empty());
 check(!nativeDependencyReady(0x431410)&&!nativeDependencyReady(0x577D90));
}
static void commandChecks(){
 seed();auto result=command(ClearBeacons);check(result.result==1&&result.count==1&&nativeCalls==1&&lastNative==0x431410&&!crcBegins);
 unknown(0x431410);nativeCalls=crcBegins=crcEnds=0;result=command(ClearBeacons);check(result.result==-12&&!result.count);expectNoNative();
 seed();result=command(RevealMap);check(result.result==1&&result.count==1&&nativeCalls==1&&lastNative==0x577D90&&crcBegins==1&&crcEnds==1);
 unknown(0x577D90);nativeCalls=crcBegins=crcEnds=0;result=command(RevealMap);check(result.result==-12&&!result.count);expectNoNative();
 seed();unknown(0x431410);unknown(0x577D90);check(signatures());
 for(int c:{ClearBeacons,RevealMap}){result=command(c);check(result.result==-12&&!result.count);expectNoNative();}
 for(auto at:{0x431410u,0x577D90u}){seed();memory.erase(at);result=command(at==0x431410?ClearBeacons:RevealMap);check(result.result==-12&&!result.count);expectNoNative();}
 seed();storage.flags=0;result=command(RevealMap);check(result.result==-21);expectNoNative();
}
static void directMapChecks(){
 seed();storage.renderReveal=3;unknown(0x431410);unknown(0x577D90);const auto before=memory;
 check(signatures());renderRevealTick(false);check(snap.renderRevealStatus==-2&&!patchPreflights&&!patchInstalls&&!patchAttempts&&memory==before);expectNoNative();
 // Enable native mode, then mutate Reveal before the next tick. Its owned
 // legacy patches must be removed without another enable/preflight attempt.
 seed();storage.renderReveal=3;renderRevealTick(false);check(snap.renderRevealStatus==1&&patchInstalls==5&&codeHookRegistry.owned.size()==5);
 unknown(0x577D90);const unsigned preflights=patchPreflights;renderRevealTick(false);
 check(snap.renderRevealStatus==-2&&patchInstalls==5&&patchRestores==5&&codeHookRegistry.owned.empty()&&patchPreflights==preflights);
 for(const auto&s:directpatch::Sites)if(s.group==directpatch::Group::Map){uint8_t actual[6]{};check(read(s.address,actual,s.size)&&!std::memcmp(actual,s.before,s.size));}
 // A foreign site was never ours. Cleanup traverses actual production code
 // while leaving that site's bytes and ownership untouched.
 seed();storage.renderReveal=3;renderRevealTick(false);const auto foreign=directpatch::Sites[0].address;
 codeHookRegistry.owned.erase(foreign);codeHookRegistry.foreign.insert(foreign);memory[foreign]=0xcc;unknown(0x577D90);
 renderRevealTick(false);check(snap.renderRevealStatus==-2&&patchRestores==4&&memory[foreign]==0xcc&&codeHookRegistry.foreign.count(foreign)&&codeHookRegistry.owned.empty());
 // An owned site overwritten externally is not force-restored either.
 seed();storage.renderReveal=3;renderRevealTick(false);memory[foreign]=0xcc;unknown(0x577D90);renderRevealTick(false);
 check(snap.renderRevealStatus==-2&&patchRestores==4&&memory[foreign]==0xcc&&codeHookRegistry.owned.count(foreign));
 // Pause/observer/unknown paths execute the actual suspendedOptions function.
 // They may retire owned legacy sites but must never install the group.
 seed();storage.renderReveal=3;suspendedOptions();check(!patchPreflights&&!patchInstalls&&!patchAttempts);
 seed();storage.renderReveal=3;renderRevealTick(false);unknown(0x577D90);
 const unsigned priorInstalls=patchInstalls,priorPreflights=patchPreflights;suspendedOptions();
 check(snap.renderRevealStatus==-2&&patchRestores==5&&codeHookRegistry.owned.empty()&&patchInstalls==priorInstalls&&patchPreflights==priorPreflights);
 seed();storage.renderReveal=3;unknown(0x577D90);memory[foreign]=0xcc;codeHookRegistry.foreign.insert(foreign);suspendedOptions();
 check(!patchPreflights&&!patchAttempts&&!patchInstalls&&memory[foreign]==0xcc&&codeHookRegistry.foreign.count(foreign));
 // Switching away from legacy direct mode while paused must retire its
 // existing sites even when the newly requested mode is nonzero. Neither
 // that cleanup nor an independent mode acquires a Reveal dependency.
 for(unsigned next:{0u,1u,2u,4u,5u}){
  seed();storage.renderReveal=3;renderRevealTick(false);unknown(0x577D90);storage.renderReveal=next;
  reads.clear();const unsigned installs=patchInstalls,preflights=patchPreflights;suspendedOptions();
  check(patchRestores==5&&codeHookRegistry.owned.empty()&&patchInstalls==installs&&patchPreflights==preflights);
  bool readReveal=false;for(auto at:reads)readReveal|=at==0x577D90;check(!readReveal);
 }
}
static void legacyBeaconRoutingChecks(){
 // Exercise the unchanged known MRGA entry routing at its prior ASLR test
 // bases. The private captured body remains covered by the existing
 // platform_startup_runtime.py fixture, rather than fabricating a valid body.
 for(uint32_t base:{0x40000000u,0x68000000u}){
  uint8_t entry[]={0xe9,0,0,0,0,0x60,0x85,0xc0};
  const uint32_t wanted=base+startupcompat::BeaconRva;
  const uint32_t delta=wanted-startupcompat::BeaconEntry-5;
  std::memcpy(entry+1,&delta,sizeof delta);uint32_t target=0;
  check(startupcompat::beaconTarget(entry,sizeof entry,base,target)&&target==wanted);
  entry[5]^=1;check(!startupcompat::beaconTarget(entry,sizeof entry,base,target));
 }
 const uint8_t currentUnknown[]={0xe9,0xbb,0x28,0x49,0x55,0x60,0x85,0xc0};uint32_t target=0;
 check(!startupcompat::beaconTarget(currentUnknown,sizeof currentUnknown,0x40000000,target));
}
int main(){
 dependencyChecks();commandChecks();directMapChecks();legacyBeaconRoutingChecks();
 std::printf("PASS: %u production startup dependency, per-command signature, CRC ordering and owned/foreign legacy-map cleanup checks; synthetic APIs/memory only\n",checks);
}
