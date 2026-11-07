// Production admission/consumer functions; synthetic PE32 images and Win32
// memory boundaries. No downloaded DLL or game machine code is executed.
#define main priorNativeFixtureMain
#include "native_entry_runtime.cpp"
#undef main
#ifndef PROFILE_BASELINE
#include "../source/native_platform_profile.h"
#endif
#include <type_traits>

constexpr uint32_t Platform=0x16000000,StubBase=0x17000000,HookTable=Platform+0xC000;
constexpr uint32_t Nt=Platform+0x100,SectionTable=Nt+24+224;
constexpr uint32_t ExportDirectory=Platform+0x8000,FunctionTable=Platform+0x8100,NameTable=Platform+0x8200,OrdinalTable=Platform+0x8300;
struct FixtureHook {uint32_t entry,site,stub,callback;unsigned prefix,length,span;const char*name;uint8_t original[12];};
// Contract fixtures independently transcribed from Phobos ce864b03 source and
// the existing vanilla entry signatures. The hook symbol is not called here.
const FixtureHook Hooks[]={
 {0x4F7870,0x4F7877,StubBase+0x100,Platform+0x2100,7,5,12,"HouseClass_CanBuild_UpgradesInteraction_WithoutAres",{0x83,0xEC,0x3C,0x8A,0x44,0x24,0x44,0x53,0x55,0x8B,0xE9,0x56}},
 {0x48DC90,0x48DC90,StubBase+0x200,Platform+0x2200,0,5,8,"MapClass_UnselectAll_ClearLimboLaunchers",{0xA1,0xC8,0xEC,0xA8,0,0x85,0xC0,0x74}},
 {0x6FBFA0,0x6FBFA3,StubBase+0x300,Platform+0x2300,3,6,9,"TechnoClass_Select_SkipLimboDelivery",{0x56,0x8B,0xF1,0x8A,0x86,0x1B,0x04,0,0}},
 {0x4F8361,0x4F8361,StubBase+0x400,Platform+0x2400,0,3,5,"HouseClass_CanBuild_UpgradesInteraction",{0xC2,0x0C,0,0x6E,0x7D}}
};
// SyringeEx 7dbb780 SyringeDebugger.cpp code_call, POPFD_POPAD expansion.
// Original register/flag/ESP restoration and branch displacement are literal
// upstream bytes; only origin and CALL rel32 are filled by the fixture.
const uint8_t LoaderWrapper[]={
 0x60,0x9C,0x68,0,0,0,0,0x54,0xE8,0,0,0,0,0x83,0xC4,0x08,
 0x64,0xA3,0x14,0,0,0,0x64,0x83,0x3D,0x14,0,0,0,0,0x74,0x18,
 0x9D,0x5F,0x5E,0x5D,0x5B,0x8B,0x44,0x24,0x0C,0x89,0x5C,0x24,0x0C,0x5B,0x5A,0x59,0x5C,
 0x64,0xFF,0x25,0x14,0,0,0,
 0x9D,0x5F,0x5E,0x5D,0x5B,0x8B,0x44,0x24,0x0C,0x89,0x5C,0x24,0x0C,0x5B,0x5A,0x59,0x5C
};
static void zero(uint32_t at,unsigned bytes){for(unsigned i=0;i<bytes;++i)memory[at+i]=0;}
static void textAt(uint32_t at,const char*text){put(at,text,std::strlen(text)+1);}
static void branch(uint32_t at,uint32_t to,uint8_t opcode=0xE9){memory[at]=opcode;put<uint32_t>(at+1,to-at-5);}
static uint32_t symbolName(unsigned index){return Platform+0x8400+index*128;}
static void install(const FixtureHook&hook){
 put(hook.entry,hook.original,hook.span);branch(hook.site,hook.stub);
 for(unsigned i=5;i<hook.length;++i)memory[hook.site+i]=0x90;
 put(hook.stub,LoaderWrapper,sizeof LoaderWrapper);put<uint32_t>(hook.stub+3,hook.site);branch(hook.stub+8,hook.callback,0xE8);
 put(hook.stub+sizeof LoaderWrapper,hook.original+hook.prefix,hook.length);
 branch(hook.stub+sizeof LoaderWrapper+hook.length,hook.site+5);
 const uint8_t callback[]={0x55,0x8B,0xEC,0x53,0x56,0x57};put(hook.callback,callback,sizeof callback);
}
static unsigned scenarios=0,nativeCalls=0;
static uint32_t calledEntry=0,receiver=0;static std::vector<uintptr_t>arguments;
static bool gateValue=false,reservationValue=false,leased=true;
template<class T>static uintptr_t argument(T value){if constexpr(std::is_pointer<T>::value)return uintptr_t(value);else return uintptr_t(value);}
template<class T,class... Args>static T call(uint32_t entry,uint32_t object,Args...args){++nativeCalls;calledEntry=entry;receiver=object;arguments={argument(args)...};return T(1);}
static bool techGate(uint32_t,uint32_t){return gateValue;}
static bool techParameterReservationAllowed(uint32_t,uint32_t){return reservationValue;}
namespace gameentry {enum class Client {Assist};static bool active(Client){return leased;}}
static void fakeNativeUnselect(){++nativeCalls;calledEntry=0x48DC90;receiver=0;arguments.clear();}
#include PROFILE_CONSUMERS

static void seedProfile(){
 seed(false);++scenarios;nativeCalls=0;calledEntry=receiver=0;arguments.clear();gateValue=reservationValue=false;leased=true;
 debugentries::applyPolicy(debugentries::Config{});for(auto&o:debugentries::observations)o={};
 addRegion(Platform,0x10000,MEM_IMAGE,PAGE_EXECUTE_READ);addRegion(StubBase,0x1000,MEM_PRIVATE,PAGE_EXECUTE_READ);
 zero(Platform,0x10000);zero(StubBase,0x1000);
 put<uint16_t>(Platform,0x5A4D);put<uint32_t>(Platform+60,0x100);put<uint32_t>(Nt,0x4550);
 put<uint16_t>(Nt+4,0x14C);put<uint16_t>(Nt+6,3);put<uint32_t>(Nt+8,0x69123456);put<uint16_t>(Nt+20,224);put<uint16_t>(Nt+22,0x2102);
 put<uint16_t>(Nt+24,0x10B);put<uint32_t>(Nt+24+56,0x10000);put<uint32_t>(Nt+24+92,16);
 put<uint32_t>(Nt+24+96,0x8000);put<uint32_t>(Nt+24+100,0x1000);
 textAt(SectionTable,".text");put<uint32_t>(SectionTable+8,0x4000);put<uint32_t>(SectionTable+12,0x2000);put<uint32_t>(SectionTable+36,0x60000020);
 textAt(SectionTable+40,".rdata");put<uint32_t>(SectionTable+48,0x6000);put<uint32_t>(SectionTable+52,0x6000);put<uint32_t>(SectionTable+76,0x40000040);
 put(SectionTable+80,".syhks00",8);put<uint32_t>(SectionTable+88,64);put<uint32_t>(SectionTable+92,0xC000);put<uint32_t>(SectionTable+116,0xC0000040);
 const char*names[]={"ESPModification",Hooks[3].name,Hooks[0].name,Hooks[1].name,"ReladdrInstructionFixup",Hooks[2].name,"ZFPreservation"};
 const uint32_t targets[]={Platform+0x7000,Hooks[3].callback,Hooks[0].callback,Hooks[1].callback,Platform+0x7001,Hooks[2].callback,Platform+0x7002};
 put<uint32_t>(ExportDirectory+20,7);put<uint32_t>(ExportDirectory+24,7);put<uint32_t>(ExportDirectory+28,0x8100);put<uint32_t>(ExportDirectory+32,0x8200);put<uint32_t>(ExportDirectory+36,0x8300);
 for(unsigned i=0;i<7;++i){textAt(symbolName(i),names[i]);put<uint32_t>(NameTable+i*4,symbolName(i)-Platform);put<uint16_t>(OrdinalTable+i*2,uint16_t(i));put<uint32_t>(FunctionTable+i*4,targets[i]-Platform);}
 const unsigned nameIndex[]={2,3,5,1};
 for(unsigned i=0;i<4;++i){put<uint32_t>(HookTable+i*16,Hooks[i].site);put<uint32_t>(HookTable+i*16+4,Hooks[i].length);put<uint32_t>(HookTable+i*16+8,symbolName(nameIndex[i]));install(Hooks[i]);}
 memory[Platform+0x7000]=memory[Platform+0x7001]=memory[Platform+0x7002]=1;
}
static void restorePhobosCanBuild(){put(Hooks[0].site,Hooks[0].original+7,5);put(Hooks[3].site,Hooks[3].original,5);}

#ifndef PROFILE_BASELINE
static bool admitted(unsigned which){return nativeDependencyReady(Hooks[which].entry,true);}
static nativeplatform::Inspection&diagnostic(unsigned which){return nativeOrdinaryInspections[which];}
static void refused(unsigned which,nativeplatform::Failure why){
 check(!admitted(which));check(diagnostic(which).failure==why);
 auto*o=debugentries::observation(debugentries::Kind::Call,Hooks[which].entry);
 check(o&&o->signature==debugentries::Signature::Mismatch&&o->hook==debugentries::Hook::NA&&o->error==-nativeplatform::ErrorBase-int(why));
 check(signatures());
}
static void positiveConsumers(){
 seedProfile();const auto before=memory;
 // The fixture's module-name API reports no Phobos.dll at all: actual target
 // ownership plus PE metadata works for a renamed module, not DLL presence.
 check(GetModuleHandleW(L"Phobos.dll")==nullptr);
 for(unsigned i=0;i<3;++i){check(admitted(i));const auto&d=diagnostic(i);check(d.failure==nativeplatform::Failure::None&&d.module==Platform&&d.callback==Hooks[i].callback&&d.stub==Hooks[i].stub&&d.observedBytes==Hooks[i].span);}
 check(memory==before);check(!nativeEntryCallable(0x4F7870,true));check(!nativeEntryCallable(0x48DC90,true));check(!nativeEntryCallable(0x6FBFA0,true));
 check(reservationCanBuild(0x20000,0x30000));check(calledEntry==0x4F7870&&receiver==0x30000&&arguments==std::vector<uintptr_t>({0x20000,0,1}));
 gateValue=true;check(reservationCanBuild(0x20000,0x30000));check(arguments[1]==1);gateValue=false;reservationValue=true;check(reservationCanBuild(0x20000,0x30000)&&arguments[1]==1);
 check(selection(0x40000));check(calledEntry==0x6FBFA0&&receiver==0x40000&&arguments.empty());unselect();check(calledEntry==0x48DC90&&receiver==0);
 const unsigned calls=nativeCalls;leased=false;unselect();check(nativeCalls==calls);check(!reservationCanBuild(0,0x30000));
 check(nativeVerifiedCanBuildDependency(0x4F7870));check(!nativeVerifiedCanBuildDependency(0x48DC90));
 check(!nativeOrdinaryEntryKnown(0x4F7877)&&!nativeOrdinaryEntryKnown(0x6A9316));
 uint8_t back[5]{};uint32_t destination=0;
 check(memory[Hooks[2].site+5]==0x90);check(read(Hooks[2].stub+sizeof LoaderWrapper+6,back,5));
 check(nativeplatform::target(Hooks[2].stub+sizeof LoaderWrapper+6,back,destination)&&destination==Hooks[2].site+5);
}
static void lifecycle(){
 seedProfile();check(admitted(0));restorePhobosCanBuild();check(admitted(0));check(nativeEntryCallable(0x4F7870,true));
 auto*o=debugentries::observation(debugentries::Kind::Call,0x4F7870);check(o->signature==debugentries::Signature::Match&&o->reason==debugentries::Reason::None&&o->error==0);
 // Companion already retired is valid; the primary callback's rewrite is
 // then idempotent. Its restored five bytes are checked twice.
 seedProfile();put(Hooks[3].site,Hooks[3].original,5);check(admitted(0));
 seedProfile();aresModule=AresBase;branch(Hooks[0].entry,Targets[2]);memory[Hooks[0].entry+5]=memory[Hooks[0].entry+6]=0x90;
 check(admitted(0));check(nativeEntryCallable(0x4F7870,true));check(admitted(1)&&admitted(2));
 // Ares being present without replacing CanBuild cannot authorize arbitrary
 // +7 bytes. The exact Phobos path is still checked, including its companion.
 seedProfile();aresModule=AresBase;check(admitted(0));memory[Hooks[3].site]=0xCC;refused(0,nativeplatform::Failure::Companion);
 seedProfile();branch(Hooks[3].stub+8,Targets[0],0xE8);refused(0,nativeplatform::Failure::Companion);
 check(!nativeEntryCallable(0x4F7870,true));
 seedProfile();branch(Hooks[3].stub+sizeof LoaderWrapper+3,Hooks[3].site+3);refused(0,nativeplatform::Failure::Companion);
}
static void wrapperRejections(){
 using F=nativeplatform::Failure;
 for(unsigned which=0;which<3;++which){
  for(unsigned i=0;i<sizeof LoaderWrapper;++i){if((i>=3&&i<7)||(i>=9&&i<13))continue;
   seedProfile();memory[Hooks[which].stub+i]^=1;refused(which,F::StubShape);
  }
  seedProfile();put<uint32_t>(Hooks[which].stub+3,Hooks[which].site+1);refused(which,F::StubShape);
  for(unsigned i=0;i<Hooks[which].length;++i){seedProfile();memory[Hooks[which].stub+sizeof LoaderWrapper+i]^=1;refused(which,F::Replay);}
  seedProfile();branch(Hooks[which].stub+sizeof LoaderWrapper+Hooks[which].length,Hooks[which].site+6);refused(which,F::Continuation);
  seedProfile();put(Hooks[which].stub+sizeof LoaderWrapper,LoaderWrapper,sizeof LoaderWrapper);refused(which,F::UnknownCallbackChain);
  seedProfile();branch(Hooks[which].stub+8,Hooks[(which+1)%3].callback,0xE8);refused(which,F::CallbackOwner);
  seedProfile();branch(Hooks[which].stub+8,Targets[0],0xE8);refused(which,F::ModuleIdentity);
  seedProfile();branch(Hooks[which].stub+8,AresBase+0x1000,0xE8);refused(which,F::ModuleLayout);
  seedProfile();memory[Hooks[which].callback]=0xC2;refused(which,F::CallbackShape);
  seedProfile();branch(Hooks[which].callback,Targets[0]);refused(which,F::CallbackShape);
  seedProfile();const uint8_t pushRet[]={0x68,0,0,0,0x60,0xC3};put(Hooks[which].callback,pushRet,sizeof pushRet);refused(which,F::CallbackShape);
  seedProfile();const uint8_t movJump[]={0xB8,0,0,0,0x60,0xFF,0xE0};put(Hooks[which].callback,movJump,sizeof movJump);refused(which,F::CallbackShape);
  seedProfile();const uint8_t movPushRet[]={0xB9,0,0,0,0x60,0x51,0xC3};put(Hooks[which].callback,movPushRet,sizeof movPushRet);refused(which,F::CallbackShape);
  seedProfile();branch(Hooks[which].site,0x80000000);refused(which,F::SiteJump);
  seedProfile();branch(Hooks[which].site,StubBase+4090);refused(which,F::StubUnreadable);
  seedProfile();memory.erase(Hooks[which].stub+sizeof LoaderWrapper+Hooks[which].length+4);refused(which,F::StubUnreadable);
 }
 for(unsigned which:{0u,2u})for(unsigned i=0;i<Hooks[which].prefix;++i){seedProfile();memory[Hooks[which].entry+i]^=1;refused(which,F::EntryLayout);}
 seedProfile();memory[Hooks[1].entry+7]^=1;refused(1,F::EntryLayout);
 seedProfile();memory[Hooks[2].site+5]=0x91;refused(2,F::SiteJump);
}
static void metadataRejections(){
 using F=nativeplatform::Failure;
 for(unsigned which=0;which<3;++which){
  seedProfile();put<uint32_t>(HookTable+which*16+4,Hooks[which].length+1);refused(which,F::HookDeclaration);
  seedProfile();put<uint32_t>(HookTable+which*16+8,AresBase+0x1000);refused(which,F::HookDeclaration);
  seedProfile();put<uint32_t>(HookTable+which*16,Hooks[which].site+1);refused(which,F::HookDeclaration);
  seedProfile();uint8_t duplicate[16]{};check(read(HookTable+which*16,duplicate,sizeof duplicate));put(HookTable+64,duplicate,sizeof duplicate);put<uint32_t>(SectionTable+88,80);refused(which,F::HookDeclaration);
  seedProfile();memory[Platform]=0;refused(which,F::ModuleLayout);
  seedProfile();put<uint16_t>(Nt+4,0x8664);refused(which,F::ModuleLayout);
  seedProfile();put<uint16_t>(Nt+24,0x20B);refused(which,F::ModuleLayout);
  seedProfile();put<uint16_t>(Nt+22,0x102);refused(which,F::ModuleLayout);
  seedProfile();put<uint32_t>(Platform+60,4097);refused(which,F::ModuleLayout);
  seedProfile();put<uint16_t>(Nt+6,97);refused(which,F::ModuleLayout);
  seedProfile();put<uint32_t>(Nt+24+56,0xFFFFFFFF);refused(which,F::ModuleLayout);
  seedProfile();put<uint32_t>(SectionTable+92,0xFFFFFFF0);refused(which,F::ModuleLayout);
  seedProfile();put<uint32_t>(SectionTable+88,65);refused(which,F::ModuleLayout);
  seedProfile();memory[SectionTable+80]='X';refused(which,F::ModuleLayout);
  seedProfile();put<uint32_t>(Nt+24+96,0xFFFF);refused(which,F::ModuleLayout);
  seedProfile();put<uint32_t>(ExportDirectory+24,0xFFFFFFFF);refused(which,F::Export);check(readCalls<100);
  seedProfile();put<uint32_t>(ExportDirectory+28,0xFFFF);refused(which,F::Export);
  seedProfile();put<uint32_t>(ExportDirectory+32,0xFFFFFFF0);refused(which,F::Export);
  seedProfile();const unsigned symbol[]={2,3,5};put<uint16_t>(OrdinalTable+symbol[which]*2,7);refused(which,F::Export);
  seedProfile();put<uint32_t>(FunctionTable+symbol[which]*4,0x8000+32);refused(which,F::Export);
  seedProfile();memory[Platform+0x7001]=0;refused(which,F::Capabilities);
  seedProfile();regions[regions.size()-2].Protect=PAGE_READONLY;refused(which,F::CallbackOwner);
  seedProfile();regions[regions.size()-2].Type=MEM_PRIVATE;refused(which,F::ModuleIdentity);
  seedProfile();regions.back().Protect=PAGE_READWRITE;refused(which,F::StubUnreadable);
 }
}
static void recoveryAndDiagnostics(){
 using F=nativeplatform::Failure;using K=debugentries::Kind;using R=debugentries::Reason;
 seedProfile();memory[Hooks[0].stub+15]=4;refused(0,F::StubShape);
 auto*o=debugentries::observation(K::Call,Hooks[0].entry);check(o->reason==R::Structural&&diagnostic(0).observedBytes==12&&diagnostic(0).observed[7]==0xE9);
 restorePhobosCanBuild();check(admitted(0));check(o->signature==debugentries::Signature::Match&&o->reason==R::None&&o->error==0);
 seedProfile();memory[Hooks[0].stub+15]=4;refused(0,F::StubShape);
 debugentries::Config config;check(debugentries::setSkip(config,debugentries::id(K::Call,Hooks[0].entry),true));check(debugentries::applyPolicy(config));
 check(nativeDependencyReady(Hooks[0].entry));o=debugentries::observation(K::Call,Hooks[0].entry);check(o->signature==debugentries::Signature::Skipped&&o->reason==R::None&&o->error==0);
 check(!nativeVerifiedCanBuildDependency(Hooks[0].entry));check(!nativeEntryCallable(Hooks[0].entry,true));check(o->signature==debugentries::Signature::Skipped&&o->error==0);
 check(debugentries::setSkip(config,debugentries::id(K::Call,Hooks[0].entry),false));check(debugentries::applyPolicy(config));check(!nativeDependencyReady(Hooks[0].entry));check(o->error==-9306);
 seedProfile();check(nativeDependencyReady(Hooks[1].entry));const unsigned readsBefore=readCalls;
 for(unsigned i=0;i<64;++i)check(nativeDependencyReady(Hooks[1].entry));check(readCalls==readsBefore);
 memory[Hooks[1].stub+15]=4;clockMs+=250;check(!nativeDependencyReady(Hooks[1].entry));memory[Hooks[1].stub+15]=8;check(!nativeDependencyReady(Hooks[1].entry));clockMs+=250;check(nativeDependencyReady(Hooks[1].entry));
 seedProfile();bool changed=false;nativeplatform::Inspection detail;
 const auto mutatingRead=[&](uint32_t at,void*out,size_t bytes){const bool ok=read(at,out,bytes);if(!changed&&at==HookTable){memory[Hooks[1].site+1]^=1;changed=true;}return ok;};
 check(!nativeplatform::inspect(Hooks[1].entry,detail,mutatingRead,nativeExecutableRange,nativeImageAllocation,nativeImageOwned));check(changed&&detail.failure==F::Changed);
 seedProfile();put(Hooks[3].site,Hooks[3].original,5);changed=false;
 const auto companionRead=[&](uint32_t at,void*out,size_t bytes){const bool ok=read(at,out,bytes);if(!changed&&at==Hooks[3].site){memory[Hooks[3].site]=0xCC;changed=true;}return ok;};
 check(!nativeplatform::inspect(Hooks[0].entry,detail,companionRead,nativeExecutableRange,nativeImageAllocation,nativeImageOwned));check(changed&&detail.failure==F::Changed);
 seedProfile();changed=false;unsigned mainReads=0;
 const auto wrappedCompanionRead=[&](uint32_t at,void*out,size_t bytes){const bool ok=read(at,out,bytes);if(at==Hooks[0].site&&++mainReads==2){memory[Hooks[3].stub+15]=4;changed=true;}return ok;};
 check(!nativeplatform::inspect(Hooks[0].entry,detail,wrappedCompanionRead,nativeExecutableRange,nativeImageAllocation,nativeImageOwned));check(changed&&detail.failure==F::Changed);
}
#endif
int main(int argc,char**argv){
#ifdef PROFILE_BASELINE
 if(argc!=2)return 2;seedProfile();const int which=std::atoi(argv[1]);
 if(which==0)check(reservationCanBuild(0x20000,0x30000));
 else if(which==1){unselect();check(nativeCalls==1);}
 else if(which==2)check(selection(0x40000));else return 2;
 std::puts("UNEXPECTED baseline consumer accepted");return 0;
#else
 (void)argc;(void)argv;positiveConsumers();lifecycle();wrapperRejections();metadataRejections();recoveryAndDiagnostics();
 std::printf("PASS: %u checks, %u scenarios; production ordinary calls, PE/export/hook ownership, SyringeEx ABI/continuation, Phobos lifecycle, original call targets and raw DR separation; synthetic boundaries only\n",checks,scenarios);
#endif
}
