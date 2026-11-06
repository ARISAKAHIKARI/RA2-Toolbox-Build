// Real production startup functions and native-entry adapter, with synthetic
// Win32 boundaries. Runtime(10) supplies entry bytes/addresses, not stub bodies.
#ifdef NDEBUG
#error Assertions must remain enabled
#endif
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <map>
#include <vector>
#include "../source/signatures.h"
#include "../source/startup_dependency_policy.h"
#include "../source/startup_signature_compat.h"
using DWORD=uint32_t;
static unsigned checks=0,queries=0,readCalls=0,moduleQueries=0;
static void check(bool value){++checks;if(!value){std::fprintf(stderr,"native entry check %u failed\n",checks);std::abort();}}
struct IMAGE_DOS_HEADER {uint16_t e_magic=0;int32_t e_lfanew=0;};
struct IMAGE_NT_HEADERS32 {uint32_t Signature=0;struct {uint16_t Machine=0;}FileHeader;struct {uint16_t Magic=0;uint32_t SizeOfImage=0;}OptionalHeader;};
struct MEMORY_BASIC_INFORMATION {void*BaseAddress=nullptr,*AllocationBase=nullptr;size_t RegionSize=0;unsigned State=0,Type=0,Protect=0;};
constexpr unsigned IMAGE_DOS_SIGNATURE=0x5a4d,IMAGE_NT_SIGNATURE=0x4550,IMAGE_FILE_MACHINE_I386=0x14c,IMAGE_NT_OPTIONAL_HDR32_MAGIC=0x10b;
constexpr unsigned MEM_COMMIT=0x1000,MEM_IMAGE=0x1000000,MEM_PRIVATE=0x20000,PAGE_NOACCESS=1,PAGE_READONLY=2,PAGE_READWRITE=4,PAGE_GUARD=0x100,PAGE_EXECUTE=0x10,PAGE_EXECUTE_READ=0x20,PAGE_EXECUTE_READWRITE=0x40,PAGE_EXECUTE_WRITECOPY=0x80;
static std::map<uint32_t,uint8_t> memory;
static std::vector<MEMORY_BASIC_INFORMATION> regions;
static uint32_t clockMs=1000,aresModule=0;
static bool read(uint32_t address,void*out,size_t bytes){
 ++readCalls;for(size_t i=0;i<bytes;++i){auto it=memory.find(address+uint32_t(i));if(it==memory.end())return false;static_cast<uint8_t*>(out)[i]=it->second;}return true;
}
static void put(uint32_t address,const void*data,size_t bytes){for(size_t i=0;i<bytes;++i)memory[address+uint32_t(i)]=static_cast<const uint8_t*>(data)[i];}
template<class T>static void put(uint32_t address,T value){put(address,&value,sizeof value);}
static size_t VirtualQuery(void*p,MEMORY_BASIC_INFORMATION*out,size_t bytes){
 ++queries;if(bytes!=sizeof(*out))return 0;const auto address=uintptr_t(p);
 for(const auto&region:regions)if(address>=uintptr_t(region.BaseAddress)&&address<uintptr_t(region.BaseAddress)+region.RegionSize){*out=region;return sizeof(*out);}return 0;
}
static void*GetModuleHandleW(const wchar_t*name){++moduleQueries;return !std::wcscmp(name,L"Ares.dll")?(void*)(uintptr_t)aresModule:nullptr;}
static uint32_t GetTickCount(){return clockMs;}
#include "../source/native_entry_runtime.h"
#include STARTUP_HELPERS
static bool checked=false,good=false,reached=false;
static DWORD signatureRetryAt=0;
static unsigned published=0;
struct {int status=1,commandStatus=0;}snap;
static void publishSnapshot(uint32_t){++published;}
static void activation(uint32_t now){
#include STARTUP_GATE
 reached=true;snap.commandStatus=1;
}
constexpr uint32_t AresBase=0x71220000;
constexpr uint32_t Entries[]={0x6AA600,0x5F7900,0x4F7870};
constexpr uint32_t Targets[]={0x08310000,0x06CF0000,0x052E0000};
constexpr uint8_t Runtime10Bytes[][8]={
 {0xE9,0xFB,0x59,0xC6,0x07,0x81,0xEC,0x44},
 {0xE9,0xFB,0x86,0x6F,0x06,0x56,0x57,0x8B},
 {0xE9,0x8B,0x87,0xDE,0x04,0x90,0x90,0x53}
};
static void addRegion(uint32_t base,uint32_t bytes,unsigned type,unsigned protect){
 regions.push_back({(void*)(uintptr_t)base,(void*)(uintptr_t)base,bytes,MEM_COMMIT,type,protect});
}
static void body(uint32_t address){const uint8_t synthetic[]={0x9c,0x60,0x83,0xec,0x08,0x8b};put(address,synthetic,sizeof synthetic);}
static void jump(uint32_t from,uint32_t target){uint8_t bytes[]={0xe9,0,0,0,0,0x90};uint32_t delta=target-from-5;std::memcpy(bytes+1,&delta,4);put(from,bytes,sizeof bytes);}
static void clearCache(){nativeAresCache={};for(auto&cache:nativeEntryCache)cache={};for(auto&cache:nativeDependencyCache)cache={};}
static void seed(bool platform=true){
 memory.clear();regions.clear();clockMs=1000;aresModule=platform?AresBase:0;clearCache();
 checked=good=reached=false;signatureRetryAt=published=0;snap={};queries=readCalls=moduleQueries=0;
 addRegion(0x400000,0x794000,MEM_IMAGE,PAGE_EXECUTE_READ);
 for(const auto&s:Signatures)put(s.address,s.bytes,s.length);
 IMAGE_DOS_HEADER dos;dos.e_magic=IMAGE_DOS_SIGNATURE;dos.e_lfanew=0x100;put(AresBase,dos);
 IMAGE_NT_HEADERS32 nt;nt.Signature=IMAGE_NT_SIGNATURE;nt.FileHeader.Machine=IMAGE_FILE_MACHINE_I386;nt.OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;nt.OptionalHeader.SizeOfImage=0x400000;put(AresBase+0x100,nt);
 addRegion(AresBase,0x400000,MEM_IMAGE,PAGE_READONLY);
 for(auto target:Targets){addRegion(target,4096,MEM_PRIVATE,PAGE_EXECUTE_READ);body(target);}
}
static void fieldEntries(){for(unsigned i=0;i<3;++i)put(Entries[i],Runtime10Bytes[i],8);}
static bool oldExactCore(){
 // Freeze the pre.5 global classification, not the current local policy.
 for(const auto&s:Signatures)if(s.address!=0x431410&&s.address!=0x577D90){uint8_t bytes[12]{};if(!read(s.address,bytes,s.length)||std::memcmp(bytes,s.bytes,s.length))return false;}
 return true;
}
static void fieldAndGate(){
 seed(false);check(signatures());for(auto entry:Entries)check(nativeEntryCallable(entry));
 seed();fieldEntries();check(!oldExactCore());const auto before=memory;
 for(unsigned i=0;i<3;++i){check(Entries[i]+5+nativeentry::word(Runtime10Bytes[i]+1)==Targets[i]);check(nativeEntryCallable(Entries[i],true));}
 check(signatures());activation(clockMs);check(reached&&good&&checked&&snap.commandStatus==1&&!published);check(memory==before);
 // Optional entry differences continue to affect only their own consumers.
 jump(0x431410,Targets[0]);jump(0x577D90,Targets[1]);check(signatures());check(!startupSignature(Signatures[8])&&!revealSignature());
 seed(false);fieldEntries();activation(clockMs);check(reached&&good&&checked&&snap.commandStatus==1&&!published);
 for(auto entry:Entries)check(!nativeDependencyReady(entry));
 aresModule=AresBase;clockMs+=1100;for(auto entry:Entries)check(nativeDependencyReady(entry));
 for(const auto&s:Signatures)if(!nativeentry::known(s.address)){
  seed();fieldEntries();memory[s.address]^=1;check(signatures()&&!nativeDependencyReady(s.address,true));
 }
}
static void rejectionChecks(){
 for(unsigned i=0;i<3;++i){
  seed();fieldEntries();regions[2+i].Protect=PAGE_READWRITE;check(!nativeEntryCallable(Entries[i],true));
  seed();fieldEntries();regions[2+i].Protect|=PAGE_GUARD;check(!nativeEntryCallable(Entries[i],true));
  seed();fieldEntries();regions[2+i].State=0;check(!nativeEntryCallable(Entries[i],true));
  seed();fieldEntries();regions[2+i].Type=0x40000;check(!nativeEntryCallable(Entries[i],true));
  seed();fieldEntries();memory.erase(Targets[i]+3);check(!nativeEntryCallable(Entries[i],true));
  seed();fieldEntries();memory[Targets[i]]=0;check(!nativeEntryCallable(Entries[i],true));
  seed();fieldEntries();memory[Targets[i]]=0xcc;check(!nativeEntryCallable(Entries[i],true));
  seed();fieldEntries();jump(Entries[i],0x100);check(!nativeEntryCallable(Entries[i],true));
  seed();fieldEntries();jump(Entries[i],0x80000000u);check(!nativeEntryCallable(Entries[i],true));
  seed();fieldEntries();jump(Entries[i],Targets[i]+4093);body(Targets[i]+4093);check(!nativeEntryCallable(Entries[i],true));
  seed();fieldEntries();memory[Entries[i]]=0xcc;check(!nativeEntryCallable(Entries[i],true));
 }
 seed();fieldEntries();memory[AresBase]=0;check(!nativeAresAvailable(true)&&signatures()&&!nativeDependencyReady(Entries[2],true));
 seed();fieldEntries();auto nt=IMAGE_NT_HEADERS32{};nt.Signature=IMAGE_NT_SIGNATURE;nt.FileHeader.Machine=0x8664;nt.OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;nt.OptionalHeader.SizeOfImage=0x400000;put(AresBase+0x100,nt);check(!nativeAresAvailable(true));
 seed();fieldEntries();regions[1].Type=MEM_PRIVATE;check(!nativeAresAvailable(true));
 seed();fieldEntries();regions[1].AllocationBase=(void*)(uintptr_t)(AresBase+4096);check(!nativeAresAvailable(true));
 seed();fieldEntries();regions[1].Protect|=PAGE_GUARD;check(!nativeAresAvailable(true));
 seed();fieldEntries();check(!nativeEntryCallable(0x431410,true)&&!nativeEntryCallable(0x577D90,true)&&!nativeEntryCallable(0x6ABC60,true));
}
static void chainChecks(){
 seed();fieldEntries();const uint32_t base=Targets[0];
 jump(base,base+0x40);uint8_t shortJump[]={0xeb,0x0e,0x90,0x90,0x90,0x90};put(base+0x40,shortJump,sizeof shortJump);
 uint8_t indirect[]={0xff,0x25,0,0,0,0};uint32_t pointer=0xA00000;std::memcpy(indirect+2,&pointer,4);put(base+0x50,indirect,sizeof indirect);put(pointer,base+0x20);body(base+0x20);
 check(nativeEntryCallable(Entries[0],true));
 memory.erase(pointer+3);check(!nativeEntryCallable(Entries[0],true));
 seed();fieldEntries();jump(base,base);check(!nativeEntryCallable(Entries[0],true));
 seed();fieldEntries();jump(base,base+0x20);jump(base+0x20,base);check(!nativeEntryCallable(Entries[0],true));
 seed();fieldEntries();for(unsigned i=0;i<8;++i)jump(base+32*i,base+32*(i+1));body(base+32*8);check(!nativeEntryCallable(Entries[0],true));
 seed();fieldEntries();jump(base+0x20,base);body(base);jump(Entries[0],base+0x20);check(nativeEntryCallable(Entries[0],true));
}
static void cacheChecks(){
 seed();fieldEntries();check(nativeEntryCallable(Entries[0]));const unsigned queriesBefore=queries,readsBefore=readCalls,modulesBefore=moduleQueries;
 for(unsigned i=0;i<512;++i)check(nativeEntryCallable(Entries[0]));
 check(queries==queriesBefore&&readCalls==readsBefore&&moduleQueries==modulesBefore);
 memory[Targets[0]]=0xcc;check(!nativeEntryCallable(Entries[0],true));
 body(Targets[0]);check(!nativeEntryCallable(Entries[0]));clockMs+=250;check(nativeEntryCallable(Entries[0]));
 aresModule=0;clockMs+=250;check(!nativeEntryCallable(Entries[0]));aresModule=AresBase;clockMs+=250;check(nativeEntryCallable(Entries[0]));
 seed();check(nativeDependencyReady(0x473460));const unsigned localReads=readCalls;
 for(unsigned i=0;i<512;++i)check(nativeDependencyReady(0x473460));check(readCalls==localReads);
 memory[0x473460]^=1;clockMs+=250;check(!nativeDependencyReady(0x473460));
 memory[0x473460]^=1;check(!nativeDependencyReady(0x473460));clockMs+=250;check(nativeDependencyReady(0x473460));
 check(!nativeDependencyReady(0x123450));
 nativeentry::TimedResult timer;timer.set(0xfffffff0u,true);check(timer.fresh(0x10u));check(!timer.fresh(0x100u));
}
int main(){fieldAndGate();rejectionChecks();chainChecks();cacheChecks();std::printf("PASS: %u checks; Runtime(10) E9 entries retain local native admission, independent startup, bounded dispatch, rejection and cache recovery; synthetic Win32/stub bodies only\n",checks);}
