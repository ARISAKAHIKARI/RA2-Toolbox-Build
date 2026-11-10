#include "tech_ui_breakpoint_core.h"
#include "trigger_input_policy.h"
#include "tech_ui_breakpoint_debug.h"
#include "tech_enhancement_parameter_policy.h"
#include "tech_parameter_lookup.h"
#include "control_parameter_policy.h"
#include "map_visibility_parameter_policy.h"
#include "../../shared/source/hardware_performance.h"
extern "C" int __cdecl GuardedTechUiParameter(void*,unsigned);
extern "C" int __cdecl GuardedControlWorldEligible();
extern "C" int __cdecl GuardedTechEnhancementProbe(unsigned,unsigned);
extern "C" unsigned BeginTechEnhancementProbe();extern "C" void EndTechEnhancementProbe(unsigned);
extern "C" unsigned techEnhanceProbeReturn=0;
extern "C" __declspec(naked) int __stdcall AssistTechEnhancementProbeCall(unsigned,unsigned){__asm {
 mov ecx,[esp+4]
 mov eax,[esp+8]
 push 1
 push 0
 push eax
 mov eax,4F7870h
 call eax
 ret 8
}}
namespace techparameters {
inline constexpr uint32_t FullGroup=1u;
struct TypeIdentity{uint32_t pointer=0,id=0,table=0;};
struct EnhancedIdentity{TypeIdentity type;uint32_t items=0;int index=-1,level=0;};
struct Policy{volatile LONG enabled=0;uint32_t house=0,houseID=0,scenario=0;int session=-1,frame=-1;TypeIdentity types[1024]{};volatile LONG enhanced=0,controlling=0,compareLevel=0,showing=0;EnhancedIdentity enhancement[techenhancement::Count]{};techlookup::PointerIndex<64> enhancementIndex;};
static Policy policy;static techbp::Service service;
static bool triggerDesired=false,triggerEnabled=false;static int triggerFailure=0;
static techbp::SignatureSnapshot<sizeof triggerinput::Prefix> triggerSignature;static uint32_t triggerRevision=0;
static bool triggerCompatible(bool publishSnapshot=true){
 const bool ok=techbp::debugSignatureSnapshot(triggerSignature,triggerinput::Entry,triggerinput::Entry,techbp::patternDigest(triggerinput::Prefix),read,publishSnapshot);
 if(publishSnapshot)triggerRevision=ok?debugentries::policyRevision:0;return ok;
}
static bool mapDesired=false,mapRedrawPending=false;static int mapFailure=0;static volatile LONG mapExceptions=0;static bool lastMapPresentation=false;
static bool privateDesired=false;static int privateFailure=0;
static int failure=0,enhancementFailure=0,controlFailure=0;static bool desired=false,enhancementDesired=false,controlDesired=false,enhancementCompareRequested=false,enhancementCameoBlocked=false;
// Published eligibility is refreshed on the ordinary owner-thread maintenance
// boundary. It is independent of the installed DR lease and never authorizes
// a different mode, stop/fault, or a changed entry policy in the hot callback.
static bool controlEligible=false;
static DWORD auditAt=0,retryAt=0,enhancementParameterScan=0;static uint32_t attempted=0,refusedGroups=0,resourceGroups=0;
struct FailureRetry {
 DWORD at=0;bool waiting=false;
 void reset(){at=0;waiting=false;}
 void defer(){at=GetTickCount();waiting=true;}
 bool due()const{return !waiting||DWORD(GetTickCount()-at)>=500;}
};
static FailureRetry fullFailureRetry,controlFailureRetry;
static volatile LONG fullExceptions=0,enhancementExceptions=0,controlExceptions=0;static unsigned probeDepth=0;static uint32_t probeToken=0;
static uint32_t controlAdmissionChecks=0,worldCaptures=0;
static hardwareperf::Window performanceWindow;
static constexpr uint32_t sites[techbp::SiteCount]={0x4F7870,mapvisibility::Address,0x4F7951,0x4F7B66,controlparams::Entry,singlefps::Entry,triggerinput::Entry,0x4068E0,0x4C6860,0x6521C0};
inline unsigned hash(uint32_t p){return ((p>>4)*2654435761u)&1023u;}
static techbp::SignatureSnapshot<20> nativeDraw;static techbp::SignatureSnapshot<15> nativePrune;static techbp::SignatureSnapshot<16> nativeEntry;
static techbp::SignatureSnapshot<13> enhancementLimit;static techbp::SignatureSnapshot<16> enhancementCompare,enhancementEntry;
static techbp::SignatureSnapshot<sizeof mapvisibility::Prefix> mapSignature;
static uint32_t nativePolicyRevision=0,enhancementPolicyRevision=0,controlPolicyRevision=0,mapPolicyRevision=0;
static bool nativeStructureRejected=false;
static uint32_t nativeEntryDigest(const uint8_t(&bytes)[16]){return techbp::patternDigest(bytes);}
static bool captureNativeEntry(techbp::ObservedPattern<16>&entry,bool&stable){
 // Keep the Call policy separate: this strictly verifies the stock/Ares ABI.
 // The DR override may replace only the reference decision, never an unreadable
 // entry or a different image appearing between verification and capture.
 stable=false;uint8_t before[16]{};
 if(!read(0x4F7870,before,sizeof before))return false;
 const bool matched=nativeEntryCallable(0x4F7870,true);
 entry.load(0x4F7870,nativeEntryDigest(before),read);
 stable=entry.readable&&!memcmp(before,entry.bytes,sizeof before)&&nativeExecutableRange(0x4F7870,sizeof before);
 return matched;
}
static bool nativeCompatible(bool publishSnapshots=true){
 techbp::ObservedPattern<20> draw;techbp::ObservedPattern<15> prune;techbp::ObservedPattern<16> entry;
 draw.load(0x6A97C3,0xD3C40F9Du,read);prune.load(0x6AA777,0x26F9619Eu,read);
 bool stable=false;const bool entryMatched=captureNativeEntry(entry,stable);
 const bool readable=draw.readable&&prune.readable&&entry.readable;
 bool admitted=debugentries::allow(debugentries::Kind::Breakpoint,0x4F7870,readable,draw.matched&&prune.matched&&entryMatched,
  entryMatched&&entry.readable&&nativeEntryDigest(entry.bytes)!=0x6995B50Eu?debugentries::Signature::Platform:debugentries::Signature::Match);
 nativeStructureRejected=readable&&!stable;
 if(nativeStructureRejected){admitted=false;debugentries::installation(debugentries::Kind::Breakpoint,0x4F7870,debugentries::Hook::Unavailable,debugentries::Reason::Structural);}
 if(publishSnapshots){draw.publish(nativeDraw,admitted);prune.publish(nativePrune,admitted);entry.publish(nativeEntry,admitted);nativePolicyRevision=admitted?debugentries::policyRevision:0;}
 return admitted;
}
static bool enhancementCompatible(bool publishSnapshots=true){
 techbp::ObservedPattern<13> limit;techbp::ObservedPattern<16> compare,entry;
 limit.load(0x4F794A,0x0FC7BB79u,read);compare.load(0x4F7B5C,0x15DAD6B8u,read);entry.load(0x4F7870,0x6995B50Eu,read);
 const bool limitOK=debugentries::allow(debugentries::Kind::Breakpoint,0x4F7951,limit.readable&&entry.readable,limit.matched&&entry.matched);
 const bool compareOK=debugentries::allow(debugentries::Kind::Breakpoint,0x4F7B66,compare.readable&&entry.readable,compare.matched&&entry.matched);
 const bool admitted=limitOK&&(!(sh&&sh->techEnhanceCompareLevel)||compareOK);
 if(publishSnapshots){limit.publish(enhancementLimit,admitted);compare.publish(enhancementCompare,admitted&&compareOK);entry.publish(enhancementEntry,admitted);enhancementPolicyRevision=admitted?debugentries::policyRevision:0;}
 return admitted;
}
static bool enhancementBodyBypassed(){
 // Reference overrides cannot make Ares execute skipped vanilla internals.
 uint8_t entry[16]{};return read(0x4F7870,entry,sizeof entry)&&nativeEntryDigest(entry)!=0x6995B50Eu&&
  nativeAresAvailable()&&nativeEntryCallable(0x4F7870);
}
// Hot paths compare the complete admitted image on every hit. Configuration
// changes are handled on the leased ordinary entry, never inside an exception.
static bool nativeCompatibleHot(){return nativePolicyRevision==debugentries::policyRevision&&nativeDraw.matches(reinterpret_cast<const volatile uint8_t*>(0x6A97C3))&&nativePrune.matches(reinterpret_cast<const volatile uint8_t*>(0x6AA777))&&nativeEntry.matches(reinterpret_cast<const volatile uint8_t*>(0x4F7870));}
static bool enhancementCompatibleHot(){return enhancementPolicyRevision==debugentries::policyRevision&&enhancementLimit.matches(reinterpret_cast<const volatile uint8_t*>(0x4F794A))&&(!policy.compareLevel||enhancementCompare.matches(reinterpret_cast<const volatile uint8_t*>(0x4F7B5C)))&&enhancementEntry.matches(reinterpret_cast<const volatile uint8_t*>(0x4F7870));}
static bool mapCompatible(bool publishSnapshot=true){
 const bool admitted=techbp::debugSignatureSnapshot(mapSignature,mapvisibility::Address,mapvisibility::PrefixAddress,techbp::patternDigest(mapvisibility::Prefix),read,publishSnapshot);
 if(publishSnapshot)mapPolicyRevision=admitted?debugentries::policyRevision:0;return admitted;
}
static bool controlCompatible(bool publishRevision=true){
 ++controlAdmissionChecks;
 // Capture every required byte before deciding reference admission, including
 // when an early caller differs. Repeated predicates below reuse this image
 // instead of issuing the same process-memory read up to three times.
 controlparams::AdmissionImage image;const bool readable=image.capture(read);
 auto observed=[&](uint32_t at,void*out,size_t n){return image.copy(at,out,n);};
 bool matched=readable;for(const auto&site:mastercontrol::Sites){uint8_t original[5]{};const bool ok=observed(site.address,original,sizeof original);if(!ok||!mastercontrol::original(site.address,original,sizeof original))matched=false;}
 // Patch-context overrides belong to another kind at the same addresses.
 // Use the raw reference predicate before this entry's independent DR allow.
 const bool contextMatched=mastercontrol::compatibleStrict(observed);matched=matched&&contextMatched;
 const bool admitted=debugentries::allow(debugentries::Kind::Breakpoint,controlparams::Entry,readable,matched);
 if(publishRevision)controlPolicyRevision=admitted?debugentries::policyRevision:0;return admitted;
}
static bool enhancedRequested(){return sh&&sh->techEnhance&&sh->techEnhanceImplementation==techenhanceparams::Exact&&!sh->stop&&!fault;}
static bool healthy(){return service.state!=techbp::System&&service.state!=techbp::Foreign&&service.state!=techbp::Thread;}
static bool ready(uint32_t group){return healthy()&&(service.activeTags()&group)==group;}
static uint32_t enhancementTags(){return techenhanceparams::breakpointTags(enhancementCompareRequested);}
static bool enhancementReady(){return ready(enhancementTags())&&(service.activeTags()&12u)==enhancementTags();}
static const TypeIdentity* fullIdentity(uint32_t pointer){return techlookup::findIdentity(policy.types,pointer);}
static bool fullType(uint32_t pointer){const auto*t=fullIdentity(pointer);return t&&get<uint32_t>(pointer)==t->table&&get<uint32_t>(pointer+0x10)==t->id;}
// Diagnostics follow the feature owning outstanding cleanup. A healthy
// sibling may keep the shared handler alive after this group has retired.
static bool groupOff(uint32_t group){
 if((service.lease.tags()&group)||service.lease.uncertain||service.pendingFor(group))return false;
 return (service.lease.tags()&(techbp::AllTags&~group))||!(resourceGroups&group)||!service.resources();
}
static bool retryFailure(FailureRetry&retry,uint32_t group){
 // A transient read/context failure must not disable a requested feature for
 // the rest of the match. Retry only after its old lease has really retired;
 // healthy siblings may keep the shared handler and their slots. Never run
 // this admission work in the hardware-exception callback.
 if(!retry.due()||!healthy()||!groupOff(group))return false;
 retry.defer();return true;
}
static void syncDebugBreakpointPolicy(){
 static uint32_t observed=0;static bool required[techbp::SiteCount]={true,true,true,true,true,true,true,true,true,true};
 const uint32_t revision=debugentries::policyRevision;if(observed==revision)return;observed=revision;
 bool changed[techbp::SiteCount]{};uint32_t groups=0;
 for(unsigned i=0;i<techbp::SiteCount;++i){const bool next=debugentries::signatureRequired(debugentries::Kind::Breakpoint,sites[i]);changed[i]=next!=required[i];required[i]=next;if(changed[i])groups|=1u<<i;}
 if(changed[0]){InterlockedExchange(&policy.enabled,0);desired=false;failure=0;fullFailureRetry.reset();nativePolicyRevision=0;nativeDraw.valid=nativePrune.valid=nativeEntry.valid=false;}
 else if(nativePolicyRevision)nativePolicyRevision=revision;
 if(changed[2]||changed[3]){InterlockedExchange(&policy.enhanced,0);enhancementDesired=false;enhancementFailure=0;enhancementPolicyRevision=0;enhancementLimit.valid=enhancementCompare.valid=enhancementEntry.valid=false;groups|=12u;}
 else if(enhancementPolicyRevision)enhancementPolicyRevision=revision;
 if(changed[4]){controlEligible=false;InterlockedExchange(&policy.controlling,0);controlDesired=false;controlFailure=0;controlFailureRetry.reset();controlPolicyRevision=0;}
 else if(controlPolicyRevision)controlPolicyRevision=revision;
 if(changed[1]){if(policy.showing)mapRedrawPending=true;InterlockedExchange(&policy.showing,0);mapDesired=false;mapFailure=0;mapPolicyRevision=0;mapSignature.valid=false;}
 else if(mapPolicyRevision)mapPolicyRevision=revision;
 if(changed[5]){singlefps::disable();singlefps::desired=false;singlefps::failure=0;singlefps::signatureRevision=0;singlefps::native.valid=false;}
 else if(singlefps::signatureRevision)singlefps::signatureRevision=revision;
 if(changed[6]){triggerEnabled=triggerDesired=false;triggerFailure=0;triggerRevision=0;triggerSignature.valid=false;}else if(triggerRevision)triggerRevision=revision;
 if(changed[7]||changed[8]||changed[9]){privateDesired=false;privateEnabled=false;privateFailure=0;groups|=privateout::BreakpointTags;}
 if(groups){attempted=0;refusedGroups&=~groups;}
}
static bool debugExactUnreachable=false;
static void inspectDebugBreakpointSignatures(){
 if(!sh||!sh->debugVisible||sh->stop||fault)return;
 static DWORD at=0;static uint32_t revision=0;const DWORD now=GetTickCount();
 if(revision==debugentries::policyRevision&&at&&DWORD(now-at)<500)return;
 at=now;revision=debugentries::policyRevision;
 // These probes only observe bytes; they never configure a DR, create a
 // timer/VEH, publish a callback permission, or change an owned snapshot.
 nativeCompatible(false);enhancementCompatible(false);mapCompatible(false);controlCompatible(false);singlefps::compatible(false);triggerCompatible(false);
 debugExactUnreachable=enhancementBodyBypassed();
}
static void publishDebugBreakpointStates(){
 if(!sh)return;using debugentries::Hook;using debugentries::Reason;using debugentries::Kind;
 const uint32_t active=service.activeTags(),retained=service.lease.tags();
 // This entire publication runs on the owner without engine calls or a DR
 // transaction. Read the pending/ownership view once, not once per UI row.
 const uint32_t pendingGroups=service.pendingGroups();const bool uncertain=service.lease.uncertain!=0,resources=service.resources();
 auto off=[&](uint32_t group){return !(retained&group)&&!uncertain&&!(pendingGroups&group)&&
  ((retained&(techbp::AllTags&~group))||!(resourceGroups&group)||!resources);};
 const bool requested[techbp::SiteCount]={bool((sh->flags&UnsafeTech)&&techmode::parameters(techImplementation)),bool(sh->mapVisibility),enhancedRequested(),enhancedRequested()&&bool(sh->techEnhanceCompareLevel),controlmode::parameters(sh->controlAll),sh->singlePlayerFPS>=10&&singlefps::valid(sh->singlePlayerFPS),bool(sh->keepInputUnlocked||sh->blockTriggerCamera),sh->privateOutMode==2,sh->privateOutMode==2,sh->privateOutMode==2};
 const int failures[techbp::SiteCount]={failure,mapFailure,enhancementFailure,enhancementFailure,controlFailure,singlefps::failure,triggerFailure,privateFailure,privateFailure,privateFailure};
 for(unsigned i=0;i<techbp::SiteCount;++i){const uint32_t group=1u<<i;const bool pending=(pendingGroups&group)||uncertain;
  Hook hook=Hook::Inactive;Reason reason=Reason::NoRequest;int error=0;
  if(pending){hook=Hook::Pending;reason=Reason::OwnerPending;error=int(service.lastError);}
  else if((requested[i]||(retained&group))&&service.state==techbp::Foreign){hook=Hook::Conflict;reason=Reason::Foreign;error=int(service.lastError);}
  else if((active&group)&&healthy()){hook=Hook::Installed;reason=requested[i]?Reason::None:Reason::OwnerPending;}
  else if((retained&group)||(!requested[i]&&!off(group))){hook=Hook::Pending;reason=Reason::OwnerPending;error=int(service.lastError);}
  else if((i==2||i==3)&&(enhancementFailure==techenhanceparams::NativeBodyBypassed||(sh->debugVisible&&debugExactUnreachable))){hook=requested[i]?Hook::Unavailable:Hook::Inactive;reason=Reason::PathUnreachable;}
  else if(requested[i]){
   hook=Hook::Pending;reason=Reason::AwaitRuntime;
   if(failures[i]==techbp::Signature){hook=Hook::Unavailable;const auto*o=debugentries::observation(Kind::Breakpoint,sites[i]);reason=o&&o->signature==debugentries::Signature::Unreadable?Reason::Unreadable:i==0&&nativeStructureRejected?Reason::Structural:Reason::Signature;}
   else if((refusedGroups&group)||failures[i]==techbp::Slots){hook=Hook::Unavailable;reason=Reason::Structural;error=techbp::Slots;}
   else if(failures[i]==-7){reason=Reason::OwnerPending;}
   else if((failures[i]==techbp::System||service.state==techbp::System)&&service.lastError==ERROR_NOT_SUPPORTED){hook=Hook::Unavailable;reason=Reason::Unsupported;error=ERROR_NOT_SUPPORTED;}
   else if(failures[i]||service.state<0){hook=Hook::Unavailable;reason=Reason::Structural;error=failures[i]?failures[i]:int(service.state);}
  }
  debugentries::installation(Kind::Breakpoint,sites[i],hook,reason,error);
 }
}
static void publishHardwarePerformance(){
 if(!sh)return;
 // All hot values already existed. No clock, allocation, log or additional
 // frame read is added to the exception callback for this diagnostic.
 const hardwareperf::Counters totals{uint32_t(service.exceptions),uint32_t(service.changed),uint32_t(controlExceptions),
  uint32_t(fullExceptions),uint32_t(enhancementExceptions),uint32_t(mapExceptions),controlAdmissionChecks,worldCaptures};
 hardwareperf::Snapshot next;
 if(!performanceWindow.observe(GetTickCount(),match,policy.frame,totals,next))return;
 InterlockedIncrement(&sh->hardwarePerfSeq);sh->hardwarePerf=next;MemoryBarrier();InterlockedIncrement(&sh->hardwarePerfSeq);
}
static void publish(){
 if(!sh)return;inspectDebugBreakpointSignatures();publishDebugBreakpointStates();publishHardwarePerformance();const uint32_t active=service.activeTags();
 sh->hardwareSlotBudget=service.slotBudget();
 sh->triggerInputStatus=triggerFailure?triggerFailure:triggerDesired?(ready(triggerinput::Tag)?techbp::Ready:(refusedGroups&triggerinput::Tag)?techbp::Slots:service.state<0?service.state:techbp::Waiting):groupOff(triggerinput::Tag)?techbp::Off:techbp::Waiting;
 sh->singlePlayerFPSStatus=singlefps::failure?singlefps::failure:singlefps::desired?(ready(singlefps::Tag)&&sh->singlePlayerFPSActive?techbp::Ready:(refusedGroups&singlefps::Tag)?techbp::Slots:service.state<0?service.state:techbp::Waiting):groupOff(singlefps::Tag)?techbp::Off:techbp::Waiting;
 const bool mapPresentation=policy.showing!=0;if(mapPresentation!=lastMapPresentation){mapRedrawPending=true;lastMapPresentation=mapPresentation;}
 sh->mapVisibilityStatus=mapFailure?mapFailure:mapDesired?(ready(mapvisibility::Tag)?techbp::Ready:(refusedGroups&mapvisibility::Tag)?techbp::Slots:service.state<0?service.state:techbp::Waiting):groupOff(mapvisibility::Tag)?techbp::Off:service.state<0?service.state:techbp::Waiting;
 sh->mapVisibilityError=uint32_t(service.lastError);sh->mapVisibilityExceptions=uint32_t(mapExceptions);
 sh->techParameterStatus=failure?failure:desired?(ready(FullGroup)?techbp::Ready:(refusedGroups&FullGroup)?techbp::Slots:service.state<0?service.state:techbp::Waiting):groupOff(FullGroup)?techbp::Off:service.state<0?service.state:techbp::Waiting;
 sh->techParameterError=uint32_t(service.lastError);sh->techParameterSlots=__builtin_popcount(active&FullGroup);sh->techParameterExceptions=uint32_t(fullExceptions);
 sh->techEnhanceParameterStatus=enhancementFailure?enhancementFailure:enhancementDesired?(enhancementReady()?(enhancementCameoBlocked?techbp::Signature:techbp::Ready):(refusedGroups&12)?techbp::Slots:service.state<0?service.state:techbp::Waiting):groupOff(12)?techbp::Off:service.state<0?service.state:techbp::Waiting;
 sh->techEnhanceParameterError=uint32_t(service.lastError);sh->techEnhanceParameterSlots=__builtin_popcount(active&12);sh->techEnhanceParameterExceptions=uint32_t(enhancementExceptions);
}
static bool callback(void*context,uint32_t tag){return GuardedTechUiParameter(context,tag)!=0;}
static bool captureWorld(uint32_t house){
 ++worldCaptures;
 if(!sh||sh->stop||fault||house!=lastHouse||!house||get<uint32_t>(0xA83D4C)!=house)return false;
 uint32_t scenario=0,id=0;int frame=-1,session=-1;
 if(!read(0xA8B230,&scenario,4)||!scenario||!read(0xA8ED84,&frame,4)||frame<0||!read(0xA8B238,&session,4)||!eventdelivery::mayQueue(session)||!read(house+0x10,&id,4)||!id)return false;
 if(policy.house!=house||policy.houseID!=id||policy.scenario!=scenario||policy.session!=session||frame<policy.frame){InterlockedExchange(&policy.enabled,0);InterlockedExchange(&policy.enhanced,0);controlEligible=false;InterlockedExchange(&policy.controlling,0);InterlockedExchange(&policy.showing,0);policy.enhancementIndex.reset();}
 policy.house=house;policy.houseID=id;policy.scenario=scenario;policy.frame=frame;policy.session=session;return true;
}
static bool capture(uint32_t house){
 InterlockedExchange(&policy.enabled,0);if(!captureWorld(house))return false;
 for(auto&t:policy.types)t={};for(int i=0;i<snap.typeCount&&i<MaxTypes;++i)if(techAllowed[i]&&typePointers[i]){
  uint32_t words[5]{};if(!read(typePointers[i],words,sizeof words)||!words[0]||!words[4])return false;
  unsigned n=hash(typePointers[i]);while(policy.types[n].pointer&&policy.types[n].pointer!=typePointers[i])n=(n+1)&1023u;
  policy.types[n]={typePointers[i],words[4],words[0]};
 }MemoryBarrier();return true;
}
static bool world(){
 return sh&&!sh->stop&&!fault&&get<uint32_t>(0xA83D4C)==policy.house&&get<uint32_t>(policy.house+0x10)==policy.houseID&&get<uint32_t>(0xA8B230)==policy.scenario&&
  get<int>(0xA8B238)==policy.session&&get<int>(0xA8ED84)>=policy.frame&&get<bool>(0xA8E9A0)&&policy.house!=get<uint32_t>(0xAC1198)&&!get<bool>(policy.house+0x1F5)&&!get<bool>(policy.house+0x1F6)&&!get<bool>(policy.house+0x1F8);
}
static bool refreshControlEligibility(){
 // Revoke before touching game memory. Even an unrelated propagated unwind
 // must not leave the previous publication usable by a sibling configure().
 controlEligible=false;InterlockedExchange(&policy.controlling,0);
 if(!controlDesired||controlFailure||!sh||sh->stop||fault||!controlmode::parameters(sh->controlAll)||controlPolicyRevision!=debugentries::policyRevision)return false;
 controlEligible=GuardedControlWorldEligible()!=0;return controlEligible;
}
static bool configure(){
 const uint32_t wanted=(desired&&!failure?FullGroup:0u)|(enhancementDesired&&(!enhancementFailure||enhancementFailure==-7)?enhancementTags():0u)|(controlDesired&&!controlFailure?16u:0u)|(mapDesired&&!mapFailure?mapvisibility::Tag:0u)|(singlefps::desired&&!singlefps::failure?singlefps::Tag:0u)|(triggerDesired&&!triggerFailure?triggerinput::Tag:0u)|(privateDesired&&!privateFailure?privateout::BreakpointTags:0u);service.poll();if(!service.resources())resourceGroups=0;
 if(!service.pending()){refusedGroups&=~service.activeTags();if(service.state==techbp::Slots)refusedGroups|=attempted&~service.activeTags();}
 if(service.pending()){publish();return false;}
 if(!healthy()){
  controlEligible=false;triggerEnabled=false;privateEnabled=false;
  InterlockedExchange(&policy.enabled,0);InterlockedExchange(&policy.enhanced,0);InterlockedExchange(&policy.controlling,0);
  InterlockedExchange(&policy.showing,0);if(mapDesired)mapFailure=int(service.state);
  if(controlDesired){controlFailure=int(service.state);controlFailureRetry.defer();}
  singlefps::disable();if(singlefps::desired)singlefps::failure=int(service.state);
  if(desired){failure=int(service.state);fullFailureRetry.defer();}if(enhancementDesired)enhancementFailure=int(service.state);service.retire();publish();return false;
 }
 const DWORD now=sh?sampleClock(sh):GetTickCount();
 if(!wanted){service.retire();attempted=0;retryAt=0;refusedGroups=0;}
 else if(service.activeTags()!=wanted){
  if(attempted!=wanted||!retryAt||DWORD(now-retryAt)>=500){attempted=wanted;retryAt=now;resourceGroups|=wanted;service.configure(sites,wanted,callback);}
 }else if(!auditAt||DWORD(now-auditAt)>=500){auditAt=now;service.audit();}
 // Slot exhaustion leaves the verified existing subset untouched.
 if(desired&&!failure)InterlockedExchange(&policy.enabled,ready(FullGroup)?1:0);
 if(enhancementDesired&&(!enhancementFailure||enhancementFailure==-7))InterlockedExchange(&policy.enhanced,ready(4)?1:0);
 InterlockedExchange(&policy.controlling,controlDesired&&!controlFailure&&controlEligible&&ready(16)?1:0);
 if(mapDesired&&!mapFailure)InterlockedExchange(&policy.showing,ready(mapvisibility::Tag)?1:0);
 if(singlefps::desired&&!singlefps::failure)singlefps::enabled=ready(singlefps::Tag);else singlefps::disable();
 triggerEnabled=triggerDesired&&!triggerFailure&&ready(triggerinput::Tag);
 publish();return service.activeTags()==wanted;
}
}
extern "C" int __cdecl AssistControlWorldEligible(){
 using namespace techparameters;
 const auto readCurrent=[](uint32_t address,void*out,size_t size){return controlparams::guardedRead(address,out,size);};
 return controlparams::world(policy.house,policy.houseID,policy.scenario,policy.session,policy.frame,readCurrent)?1:0;
}
static bool mapVisibilityParameter(CONTEXT&context){
 using namespace techparameters;
 if(context.Eip!=mapvisibility::Address)return false;
 techbp::countOwner(mapExceptions);
 if(!policy.showing||!sh||!sh->mapVisibility||sh->stop||fault||sh->renderReveal||(sh->flags&ExperimentalReveal)||!world())return false;
 // Local byte reads inside the existing SEH boundary avoid a syscall per tile.
 if(mapPolicyRevision!=debugentries::policyRevision||!mapSignature.matches(reinterpret_cast<const volatile uint8_t*>(uintptr_t(mapvisibility::PrefixAddress))))return false;
 context.EFlags=mapvisibility::visibleFlags(context.EFlags);return true;
}
extern "C" int __cdecl AssistTechUiParameter(void*raw,unsigned tag){
 using namespace techparameters;auto&context=*static_cast<CONTEXT*>(raw);
 if(tag>=techbp::SiteCount||context.Eip!=sites[tag])return 0;
 if(tag>=7){if(!privateEnabled||!privateDesired||!sh||sh->privateOutMode!=2||sh->stop||fault)return 0;context.Eip=privateEntry(tag);return 1;}
 if(tag==6){
  if(!triggerEnabled||!sh||sh->stop||fault||!readonly::commands(sh)||readOnlyState.active||
   !(sh->keepInputUnlocked||sh->blockTriggerCamera)||context.Ecx<0x10000||context.Esp<0x10000||context.Esp>UINT32_MAX-20)return 0;
  const int action=*reinterpret_cast<const volatile int*>(uintptr_t(context.Ecx+0x2C));
  if(!triggerinput::blocked(action,sh->keepInputUnlocked,sh->blockTriggerCamera))return 0;
  if(!world()||DWORD(sampleClock(sh)-sh->heartbeat)>=3000||triggerRevision!=debugentries::policyRevision||
   !triggerSignature.matches(reinterpret_cast<const volatile uint8_t*>(uintptr_t(triggerinput::Entry))))return 0;
  const uint32_t caller=*reinterpret_cast<const volatile uint32_t*>(uintptr_t(context.Esp));
  if(!nativeExecutableRange(caller,1))return 0;
  // thiscall bool Execute(House*,Object*,Trigger*,Cell&), ret 16. The original
  // action is presentation-only; leave trigger progression and other actions.
  context.Eax=1;context.Eip=caller;context.Esp+=20;
  if(action==46)++sh->triggerInputBlocked;else ++sh->triggerCameraBlocked;return 1;
 }
 if(tag==5)return singlefps::parameter(context)?1:0;
 if(tag==1)return mapVisibilityParameter(context)?1:0;
 if(tag==4){
  techbp::countOwner(controlExceptions);
  // Ordinary callers enter the original true leaf. The two owner queries
  // still enter its strict pointer comparison; neither leaf dereferences ECX.
  // World eligibility is a maintenance publication, not a per-hit snapshot.
  if(controlPolicyRevision!=debugentries::policyRevision||!policy.controlling||!sh||!controlmode::parameters(sh->controlAll)||sh->stop||fault||context.Esp<0x10000)return 0;
  uint32_t caller=0;if(!controlparams::guardedRead(context.Esp,&caller,4)||caller<0x10000)return 0;
  context.Eip=controlparams::destination(caller);return 1;
 }
 if(tag<2)techbp::countOwner(fullExceptions);else techbp::countOwner(enhancementExceptions);
 if(tag==0){
  // The original CALL has already pushed its return address. Classify the
  // two native UI callers before any expensive world/type queries.
  if(context.Esp<0x10000)return 0;auto*args=reinterpret_cast<uint32_t*>(uintptr_t(context.Esp));
  const uint32_t caller=args[0],pointer=args[1];const bool draw=caller==0x6A97D7,prune=caller==0x6AA786;
  if((!draw&&!prune)||pointer<0x10000||args[2]||args[3]!=(draw?0u:1u))return 0;
  if(!policy.enabled||!sh||sh->stop||fault||!(sh->flags&UnsafeTech)||!techmode::parameters(techImplementation)||context.Ecx!=policy.house)return 0;
  // Reject uncaptured types using local data before reading the live world.
  // A candidate still needs every current identity and signature guard.
  const auto*t=fullIdentity(pointer);if(!t)return 0;
  if(!world()||get<uint32_t>(pointer)!=t->table||get<uint32_t>(pointer+0x10)!=t->id)return 0;
  if(!nativeCompatibleHot()){InterlockedExchange(&policy.enabled,0);return 0;}
  args[2]=1;return 1;
 }
 if(!policy.enhanced||!sh||sh->stop||fault||!sh->techEnhance||(tag==3&&!policy.compareLevel))return 0;
 const uint32_t stack=context.Esp,returnAddress=get<uint32_t>(stack+0x4C),pointer=get<uint32_t>(stack+0x50);
 if(context.Ebp!=policy.house||get<uint32_t>(stack+0x10)!=policy.house||get<uint32_t>(stack+0x54)||
  (tag==2?context.Edi:context.Ecx)!=pointer||pointer<0x10000)return 0;
 const uint32_t include=get<uint32_t>(stack+0x58);const bool caller=(returnAddress==0x6A97D7&&include==0)||(returnAddress==0x6AA786&&include==1)||
  (probeDepth&&returnAddress==techEnhanceProbeReturn&&include==1&&probeToken&&uint32_t(InterlockedCompareExchange(&gameentry::activeToken,0,0))==probeToken);
 if(!caller)return 0;
 const unsigned i=policy.enhancementIndex.find(policy.enhancement,pointer,[](const EnhancedIdentity&t){return t.type.pointer;});
 if(i>=techenhancement::Count)return 0;
 const auto&t=policy.enhancement[i];
 if(!world()||!enhancementCompatibleHot())return 0;
 if(get<uint32_t>(pointer)!=t.type.table||get<uint32_t>(pointer+0x10)!=t.type.id||get<int>(pointer+0x634)!=t.level||
    get<uint32_t>(techenhancement::Rules[i].array+4)!=t.items||t.index<0||t.index>=get<int>(techenhancement::Rules[i].array+16)||get<uint32_t>(t.items+uint32_t(t.index)*4)!=pointer)return 0;
 context.EFlags=tag==2?techenhanceparams::limitFlags(context.EFlags):techenhanceparams::compareFlags(1,context.Ebx,context.EFlags);return 1;
}
extern "C" unsigned BeginTechEnhancementProbe(){using namespace techparameters;if(!gameentry::active(gameentry::Client::Assist)||!policy.enhanced||!enhancementReady()||probeDepth>=16)return 0;static constexpr uint8_t stub[]={0x8B,0x4C,0x24,4,0x8B,0x44,0x24,8,0x6A,1,0x6A,0,0x50,0xB8,0x70,0x78,0x4F,0,0xFF,0xD0,0xC2,8,0};
 const auto*start=reinterpret_cast<const uint8_t*>(&AssistTechEnhancementProbeCall);if(memcmp(start,stub,sizeof stub))return 0;
 techEnhanceProbeReturn=uint32_t(uintptr_t(start+20));if(!probeDepth)probeToken=uint32_t(InterlockedCompareExchange(&gameentry::activeToken,0,0));return ++probeDepth;}
extern "C" void EndTechEnhancementProbe(unsigned token){using namespace techparameters;if(token&&token==probeDepth)--probeDepth;else probeDepth=0;if(!probeDepth)probeToken=0;}
static bool mapVisibilityParametersRetired(){using namespace techparameters;return groupOff(mapvisibility::Tag);}
static bool restoreMapVisibilityParameters(){
 using namespace techparameters;const bool changed=mapDesired||(service.lease.tags()&mapvisibility::Tag)||service.lease.uncertain;
 if(policy.showing)mapRedrawPending=true;InterlockedExchange(&policy.showing,0);mapDesired=false;mapFailure=0;
 if(changed)attempted=0;refusedGroups&=~mapvisibility::Tag;configure();return mapVisibilityParametersRetired();
}
static void mapVisibilityParametersTick(uint32_t house){
 using namespace techparameters;
 if(!sh||sh->stop||fault||!sh->mapVisibility)restoreMapVisibilityParameters();
 else if(sh->renderReveal||(sh->flags&ExperimentalReveal)){restoreMapVisibilityParameters();mapFailure=-6;publish();}
 else if(!mapCompatible()){restoreMapVisibilityParameters();mapFailure=techbp::Signature;publish();}
 else if(!captureWorld(house)){InterlockedExchange(&policy.showing,0);mapFailure=techbp::Thread;publish();}
 else{mapFailure=0;mapDesired=true;configure();}
 if(mapRedrawPending&&redrawRevealTest())mapRedrawPending=false;
}
static bool techParametersRetired(){using namespace techparameters;return !(service.lease.mask&&((service.lease.tags()&FullGroup)||service.lease.uncertain))&&!service.pending()&&((service.lease.tags()&(techbp::AllTags&~FullGroup))||!service.resources());}
static bool techEnhanceParametersRetired(){using namespace techparameters;return !(service.lease.mask&&((service.lease.tags()&12)||service.lease.uncertain))&&!service.pending()&&((service.lease.tags()&(techbp::AllTags&~12u))||!service.resources());}
static bool restoreTechParameters(){using namespace techparameters;const bool changed=desired||(service.lease.tags()&FullGroup)||service.lease.uncertain;InterlockedExchange(&policy.enabled,0);desired=false;failure=0;fullFailureRetry.reset();if(changed)attempted=0;refusedGroups&=~FullGroup;configure();return techParametersRetired();}
static bool restoreTechEnhanceParameters(){using namespace techparameters;const bool changed=enhancementDesired||(service.lease.tags()&12)||service.lease.uncertain;InterlockedExchange(&policy.enhanced,0);enhancementDesired=false;enhancementFailure=0;enhancementCameoBlocked=false;if(changed)attempted=0;refusedGroups&=~12u;configure();return techEnhanceParametersRetired();}
static bool restoreTriggerInput(){using namespace techparameters;triggerEnabled=triggerDesired=false;triggerFailure=0;refusedGroups&=~triggerinput::Tag;configure();return groupOff(triggerinput::Tag);}
static void maintainTriggerInput(){
 using namespace techparameters;triggerDesired=false;triggerFailure=0;
 if(!sh||sh->stop||fault||!readonly::commands(sh)||readOnlyState.active||!(sh->keepInputUnlocked||sh->blockTriggerCamera)||
  DWORD(sampleClock(sh)-sh->heartbeat)>=3000||!lastHouse||!get<bool>(0xA8E9A0)){triggerEnabled=false;return;}
 if(!captureWorld(lastHouse)){triggerEnabled=false;return;}
 if(!triggerCompatible()){triggerEnabled=false;triggerFailure=techbp::Signature;return;}
 triggerDesired=true;
}
static void maintainTechParameters(){
 using namespace techparameters;syncDebugBreakpointPolicy();service.poll();singlefps::maintain();maintainTriggerInput();
 if(!sh||sh->stop||fault||!readonly::commands(sh)||readOnlyState.active){InterlockedExchange(&policy.enabled,0);InterlockedExchange(&policy.enhanced,0);desired=enhancementDesired=controlDesired=false;failure=enhancementFailure=controlFailure=0;enhancementCameoBlocked=false;fullFailureRetry.reset();controlFailureRetry.reset();InterlockedExchange(&policy.controlling,0);InterlockedExchange(&policy.showing,0);}
 else if(!(sh->flags&UnsafeTech)||!techmode::parameters(techImplementation)){InterlockedExchange(&policy.enabled,0);desired=false;failure=0;fullFailureRetry.reset();}
 if(!sh||sh->stop||fault||!readonly::commands(sh)||readOnlyState.active||!controlmode::parameters(sh->controlAll)){InterlockedExchange(&policy.controlling,0);controlDesired=false;controlFailure=0;controlFailureRetry.reset();}
 if(!sh||sh->stop||fault||!readonly::commands(sh)||readOnlyState.active||!sh->mapVisibility){InterlockedExchange(&policy.showing,0);mapDesired=false;mapFailure=0;}
 if(!sh||sh->stop||fault||!readonly::commands(sh)||readOnlyState.active||sh->privateOutMode!=2){privateDesired=false;if(privateMode==2)privateEnabled=false;}
 refreshControlEligibility();configure();
 // Idle slot inspection enters an actual owner-thread INT3 rendezvous and
 // briefly owns a VEH/handle even with every hardware feature disabled.
 // Only explicit diagnostics request that work. The Host already represents
 // an absent/expired budget as an unconfirmed four-slot plan; feature
 // admission and active audits still inspect the real registers themselves.
 // Stop/fault and pending restoration never start another idle inspection.
 static DWORD inspected=0;const DWORD now=GetTickCount();
 if(sh&&sh->debugVisible&&!sh->stop&&!fault&&readonly::commands(sh)&&!readOnlyState.active&&lastHouse&&get<bool>(0xA8E9A0)&&!desired&&!enhancementDesired&&!controlDesired&&!mapDesired&&!singlefps::desired&&!triggerDesired&&!service.pending()&&(!inspected||DWORD(now-inspected)>=500)){inspected=now;service.inspect();publish();}
}
static bool requestTechParameters(uint32_t house){
 using namespace techparameters;maintainTechParameters();
 if(!sh||sh->stop||fault||!(sh->flags&UnsafeTech)||!techmode::parameters(techImplementation)){publish();return false;}
 if(failure&&!retryFailure(fullFailureRetry,FullGroup)){publish();return false;}
 if(!nativeCompatible()){failure=techbp::Signature;fullFailureRetry.defer();InterlockedExchange(&policy.enabled,0);desired=false;configure();return false;}
 if(!restoreTechEnhancement()||!capture(house)){publish();return false;}
 failure=0;fullFailureRetry.reset();desired=true;configure();return ready(FullGroup)&&!failure;
}
static bool techParameterReservationAllowed(uint32_t type,uint32_t house){
 using namespace techparameters;
 return sh&&(sh->flags&UnsafeTech)&&snap.techOpen&&techmode::parameters(techImplementation)&&
  desired&&!failure&&policy.enabled&&ready(FullGroup)&&house==lastHouse&&house==policy.house&&
  world()&&fullType(type)&&nativeCompatible();
}
static int techEnhancementCatalogueLevel(uint32_t type,int level){
 if(!techparameters::enhancedRequested()||!type||(!techenhancement::eligible(techenhancement::Rules[9],level)&&level!=-1))return level;
 uint32_t table=0;char id[24]{};if(!read(type,&table,4)||!read(type+0x24,id,sizeof id)||!memchr(id,0,sizeof id))return level;
 for(const auto&r:techenhancement::Rules)if(table==r.vtable&&techenhancement::sameID(id,r.id)&&techenhancement::eligible(r,level))return 1;return level;
}
static bool techEnhancementCatalogueIncludesInsignificant(uint32_t type,uint32_t array,int index,int level){
 // PRES and BUS are deliberate enhancement targets even when their rules
 // mark them Insignificant. Admit only the active, identity-checked target
 // to our catalogue; never change that game flag or bypass native production.
 if(!sh||!sh->techEnhance||sh->stop||fault||type<0x10000||index<0)return false;
 uint32_t table=0;char id[24]{};
 if(!read(type,&table,4)||!read(type+0x24,id,sizeof id)||!memchr(id,0,sizeof id))return false;
 for(unsigned i=0;i<techenhancement::Count;++i){const auto&rule=techenhancement::Rules[i];
  if(rule.array!=array||rule.vtable!=table||!techenhancement::sameID(rule.id,id))continue;
  if(techparameters::enhancedRequested()&&techenhancement::eligible(rule,level)){
   techenhancement::Record candidate{};candidate.pointer=type;candidate.index=index;
   return read(array+4,&candidate.items,4)&&techenhancement::identity(rule,candidate,read);
  }
  const auto&record=enhancedTypes[i];
  return level==1&&record.owned&&!record.conflict&&record.pointer==type&&record.index==index&&
   enhancementWorld()==techenhancement::IdentityState::Current&&techenhancement::identity(rule,record,read);
 }
 return false;
}
static bool techEnhancementProductionPending(uint32_t house){
 // An unavailable native eligibility call cannot prove that old permission
 // is no longer needed by the factory queue. Keep that local handoff pending.
 if(!nativeDependencyReady(0x4F7870))return true;
 const unsigned offsets[]={0x53AC,0x53B0,0x53B4,0x53B8,0x53BC,0x53CC};
 auto dependent=[&](uint32_t type){return !type||(!(techparameters::desired&&techparameters::ready(techparameters::FullGroup)&&techparameters::fullType(type))&&call<int>(0x4F7870,house,(void*)(uintptr_t)type,false,true)<=0);};
 for(unsigned offset:offsets){uint32_t factory=0,product=0,items=0,owner=0;int count=0;if(!read(house+offset,&factory,4))return true;if(!factory)continue;
  if(factory<0x10000||!read(factory+0x6C,&owner,4)||owner!=house||!read(factory+0x58,&product,4)||!read(factory+0x44,&items,4)||!read(factory+0x50,&count,4)||count<0||count>128)return true;
  if(product&&(product<0x10000||dependent(typeOf(product,kind(product)))))return true;
  if(count&&(items<0x10000||uint64_t(items)+uint64_t(count)*4>UINT32_MAX))return true;
  for(int i=0;i<count;++i){uint32_t type=0;if(!read(items+uint32_t(i)*4,&type,4)||dependent(type))return true;}
 }return false;
}
static void deferTechEnhancementParameters(){
 restoreTechEnhanceParameters();techparameters::enhancementFailure=-7;techparameters::publish();
}
static void techEnhanceParametersTick(uint32_t house,DWORD now){
 using namespace techparameters;
 const bool requested=enhancedRequested();static bool catalogueRequested=false;
 if(requested!=catalogueRequested){
  // The native catalogue stamp hashes actual rules, which exact mode never
  // changes. Rebuild its effective-level filter on both request edges so a
  // midgame activation can reach the existing guarded cameo path. Readiness
  // and native CanBuild/FindFactory still gate every inserted icon below.
  catalogueRequested=requested;enhancementChangedCatalogue=true;
  catalogueDirty=true;catalogueReady=false;catalogueAt=0;scanTypes();
  if(snap.techOpen)requestTechRefresh();
 }
 if(!requested){
  if(enhancementDesired&&sh&&sh->techEnhance&&techEnhancementProductionPending(house)){enhancementFailure=-7;publish();return;}
  restoreTechEnhanceParameters();return;
 }
 if(techLegacyEnhancementProductionPending(house)){deferTechEnhancementParameters();return;}
 if(((snap.techOpen||techPending)&&!techmode::parameters(techImplementation))||techDataPending()||!nativeTextTechRetired()||!restoreTechEnhancement()){
  restoreTechEnhanceParameters();enhancementFailure=-6;publish();return;
 }
 if(enhancementBodyBypassed()){restoreTechEnhanceParameters();enhancementFailure=techenhanceparams::NativeBodyBypassed;publish();return;}
 if(!enhancementCompatible()){restoreTechEnhanceParameters();enhancementFailure=techbp::Signature;publish();return;}
 const bool compareRequested=sh->techEnhanceCompareLevel!=0;
 // Preserve in-flight products before changing the effective comparison.
 // An added slot stays inert until the owning game thread rechecks production.
 if(enhancementDesired&&compareRequested!=bool(policy.compareLevel)&&techEnhancementProductionPending(house)){enhancementFailure=-7;publish();return;}
 if(!enhancementDesired||!compareRequested)InterlockedExchange(&policy.compareLevel,compareRequested?1:0);
 enhancementCompareRequested=compareRequested;
 if(!captureWorld(house)){InterlockedExchange(&policy.enhanced,0);enhancementFailure=techbp::Thread;publish();return;}
 enhancementFailure=0;enhancementDesired=true;
 if(!enhancementParameterScan||DWORD(now-enhancementParameterScan)>=500||!policy.enhanced||!policy.enhancementIndex.ready()){
  enhancementParameterScan=now;InterlockedExchange(&policy.enhanced,0);policy.enhancementIndex.reset();for(auto&t:policy.enhancement)t={};
  // All rules for one native array share this capture, not a long-lived
  // authorization cache. Array order still determines each rule's first
  // match, and the hot callback retains its live identity/membership guards.
  for(unsigned first=0;first<techenhancement::Count;++first){const uint32_t array=techenhancement::Rules[first].array;
   bool seen=false;for(unsigned prior=0;prior<first;++prior)if(techenhancement::Rules[prior].array==array){seen=true;break;}if(seen)continue;
   unsigned remaining=0;for(unsigned i=first;i<techenhancement::Count;++i)if(techenhancement::Rules[i].array==array)++remaining;
   const Vec types=vec(array,4096);
   for(int index=0;index<types.count;++index){const uint32_t pointer=item(types,index);if(!pointer)continue;
    const int level=get<int>(pointer+0x634);if(level!=-1&&(level<2||level>100))continue;
    uint32_t table=0;char id[24]{};if(!read(pointer,&table,4)||!read(pointer+0x24,id,sizeof id)||!memchr(id,0,sizeof id))continue;
    uint32_t uid=0;bool identified=false;
    for(unsigned i=first;i<techenhancement::Count;++i){const auto&r=techenhancement::Rules[i];
     if(r.array!=array||policy.enhancement[i].type.pointer||table!=r.vtable||!techenhancement::sameID(id,r.id)||!techenhancement::eligible(r,level))continue;
     if(!identified){uid=get<uint32_t>(pointer+0x10);identified=true;}if(uid){policy.enhancement[i]={{pointer,uid,r.vtable},types.items,index,level};--remaining;}
    }
    if(!remaining)break;
   }
  }
  policy.enhancementIndex.rebuild(policy.enhancement,[](const EnhancedIdentity&t){return t.type.pointer;});MemoryBarrier();
 }
 configure();if(!enhancementReady())return;
 // Missing sidebar helpers stop this presentation pass before any icon or
 // strip write. Retain already-granted Exact permission for in-flight work;
 // a presentation-only failure must not retire its hardware group.
 enhancementCameoBlocked=!nativeDependenciesReady(0x4F7870,0x5F7900,0x6ABC60,0x6A6300);
 if(enhancementCameoBlocked){publish();return;}
 if(compareRequested!=bool(policy.compareLevel)){
  if(techEnhancementProductionPending(house)){enhancementFailure=-7;publish();return;}
  InterlockedExchange(&policy.compareLevel,compareRequested?1:0);
 }
 sidebarpolicy::CameoSet icons;if(!techCameoInventory(icons))return;
 for(const auto&r:policy.enhancement){const uint32_t type=r.type.pointer;if(!type)continue;const int i=typeSlot(type);
  if(i<0||icons.contains(snap.types[i].kind,snap.types[i].index)||GuardedTechEnhancementProbe(house,type)<=0)continue;
  uint32_t factory=techFactory(type,house);if(!factory)continue;const auto&t=snap.types[i];const int tab=typeTab(i);
  if(tab<0||tab>3||get<int>(Tabs+tab*3988+84)>=75)continue;
  if(call<bool>(0x6A6300,Sidebar,t.kind,t.index)){sidebarRedraw(tab);icons.add(t.kind,t.index);}
 }
}

// Control owns logical tag 4 in the SAME service, never a second singleton.
static bool controlParametersRetired(){return techparameters::groupOff(16);}
static bool restoreControlParameters(){
 using namespace techparameters;const bool changed=controlDesired||(service.lease.tags()&16)||service.lease.uncertain;
 controlEligible=false;InterlockedExchange(&policy.controlling,0);controlDesired=false;controlFailure=0;controlFailureRetry.reset();
 if(changed)attempted=0;refusedGroups&=~16u;configure();return controlParametersRetired();
}
static int requestControlParameters(){
 using namespace techparameters;syncDebugBreakpointPolicy();
 auto status=[](){return controlFailure?controlFailure-10:ready(16)?int(controlmode::Parameters):(refusedGroups&16)?-12:service.state<0?int(service.state)-10:0;};
 if(!sh||sh->stop||fault||!controlmode::parameters(sh->controlAll)){restoreControlParameters();return 0;}
 controlEligible=false;InterlockedExchange(&policy.controlling,0);
 if(controlFailure&&!retryFailure(controlFailureRetry,16)){publish();return status();}
 if(!controlCompatible()){controlEligible=false;InterlockedExchange(&policy.controlling,0);controlDesired=false;controlFailure=techbp::Signature;controlFailureRetry.defer();configure();return status();}
 if(!captureWorld(lastHouse)){controlEligible=false;InterlockedExchange(&policy.controlling,0);controlDesired=false;configure();return controlFailure?status():0;}
 controlFailure=0;controlFailureRetry.reset();controlDesired=true;refreshControlEligibility();configure();return status();
}


static bool singlePlayerFPSRetired(){return techparameters::groupOff(singlefps::Tag);}
static bool restoreSinglePlayerFPS(){
 using namespace techparameters;singlefps::disable();singlefps::desired=false;singlefps::failure=0;
 refusedGroups&=~singlefps::Tag;configure();
 if(!singlePlayerFPSRetired())return false;singlefps::release();publish();return true;
}


static bool privateBpRetired(){return techparameters::groupOff(privateout::BreakpointTags);}
static bool privateBpRequest(bool enabled){
 using namespace techparameters;
 if(!enabled){privateDesired=false;privateFailure=0;refusedGroups&=~privateout::BreakpointTags;configure();return privateBpRetired();}
 privateFailure=0;
 for(const auto&s:privateSites){uint8_t b[16]{};const bool readable=read(s.address,b,s.size),matched=readable&&!memcmp(b,s.before,s.size);
  if(!debugentries::allow(debugentries::Kind::Breakpoint,s.address,readable,matched)||!matched){privateFailure=techbp::Signature;break;}}
 privateDesired=privateFailure==0;configure();return privateDesired&&ready(privateout::BreakpointTags);
}

static int privateBpStatus(){
 using namespace techparameters;
 if(privateFailure==techbp::Signature)return -1;
 if(refusedGroups&privateout::BreakpointTags||service.state==techbp::Slots)return -2;
 return privateFailure||service.state<0?-3:3;
}
