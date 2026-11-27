#ifdef NDEBUG
#error assertions required
#endif
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <chrono>
#include <vector>
#include <cstring>
#include "../../external_reader/source/live_ini.h"
#include "../../external_reader/source/camera_assets.h"
#include "fixtures/live_ini_baseline/live_ini.h"
using namespace external::assets;
static std::vector<uint8_t> memory(0x1000000);
static uint64_t calls,bytes,typeNameReads;static uint32_t failAt;
constexpr uint32_t Type=0x720000;
template<class T>static void put(uint32_t p,const T&v){assert(size_t(p)+sizeof v<=memory.size());std::memcpy(memory.data()+p,&v,sizeof v);}
static void stringAt(uint32_t p,const char*value){std::memset(memory.data()+p,0,128);std::memcpy(memory.data()+p,value,std::strlen(value)+1);}
struct Read {bool operator()(uint32_t at,void*out,size_t n)const{++calls;bytes+=n;if(at==Type+0x1F8)++typeNameReads;if(at<0x10000||uint64_t(at)+n>memory.size()||(failAt>=at&&failAt<uint64_t(at)+n))return false;SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),memory.data()+at,out,n,&got)&&got==n;}
 bool readFresh(uint32_t at,void*out,size_t n){return (*this)(at,out,n);}
 bool imagePath(wchar_t*,DWORD*){return false;}
};
struct Header {uint32_t table;int32_t count,capacity;};
constexpr uint32_t Object=0x887180,Entries=0x100000,Sections=0x200000,Names=0x300000;
static unsigned checks=0;
static void check(bool v){++checks;assert(v);}
static void initialize(unsigned count){std::fill(memory.begin(),memory.end(),0);failAt=0;put(Object,uint32_t(0x7E1AF4));put(Object+0x28,Header{Entries,int(count),int(count)});for(unsigned n=0;n<count;++n){uint32_t section=Sections+n*0x50,name=Names+n*0x80;put(Entries+n*8+4,section);put(section+0xC,name);char text[24];std::snprintf(text,sizeof text,"SECTION%04u",n);stringAt(name,text);put(section+0x2C,Header{0,0,0});}}
static void installKeys(const char*palette="  LIVE \r\n"){
 put(Sections+0x2C,Header{0x400000,1,1});put(0x400004,uint32_t(0x500000));put(0x50000C,std::array<uint32_t,2>{{0x600000,0x650000}});stringAt(0x600000,"PALETTE");stringAt(0x650000,palette);
}
static double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static void measure(const char*scenario,unsigned count,int failure){
 initialize(count);if(failure==1)failAt=Names+(count-1)*0x80;if(failure==2)stringAt(Names+(count-1)*0x80,"SECTION0000");if(failure==3)put(Sections+(count-1)*0x50+0xC,uint32_t(0));
 BeforeLiveINI before;LiveINI after;after.beginPresentation();std::string text;uint64_t oldCalls=0,newCalls=0;double oldMs=0,newMs=0;
 auto run=[&](auto&ini,uint64_t&reads,double&elapsed){calls=0;double begin=now();for(unsigned n=0;n<45;++n){const std::string section=failure?"SECTION0000":"MISSING",key="PALETTE"+std::to_string(n%5);check(!ini.get(Read{},Object,1,section,key,text)&&text.empty());}elapsed=now()-begin;reads=calls;};
 run(before,oldCalls,oldMs);run(after,newCalls,newMs);
 const auto stats=after.indexDiagnostics();check(stats.builds==1&&stats.failures==(failure?1u:0u)&&stats.failureReuses==(failure?44u:0u));
 if(failure)check(newCalls*40<oldCalls);else check(newCalls==oldCalls);
 std::printf("PASS %s sections=%u lookups=45 ReadProcessMemory=%llu->%llu elapsed_ms=%.6f->%.6f bootstrap=%llu failed=%llu reused=%llu\n",scenario,count,(unsigned long long)oldCalls,(unsigned long long)newCalls,oldMs,newMs,(unsigned long long)stats.builds,(unsigned long long)stats.failures,(unsigned long long)stats.failureReuses);
}
static bool lookup(LiveINI&ini,uint32_t object=Object,uint64_t generation=1){std::string out;const bool found=ini.get(Read{},object,generation,"SECTION0000","PALETTE",out);if(found)check(out=="LIVE");else check(out.empty());return found;}
int main(){
 measure("healthy missing section",4096,0);measure("unreadable trailing section",4096,1);measure("duplicate trailing section name",4096,2);measure("null trailing section name",4096,3);
 // Failed full-index work is bounded only for an opted-in presentation.
 initialize(64);installKeys();failAt=Names+63*0x80;LiveINI bounded;bounded.beginPresentation();check(!lookup(bounded));const auto first=bounded.indexDiagnostics();check(first.builds==1&&first.failures==1);failAt=0;check(!lookup(bounded));check(bounded.indexDiagnostics().failureReuses==1);bounded.beginPresentation();check(lookup(bounded));check(bounded.indexDiagnostics().builds==2); // No clock or five-second generation change.
 initialize(64);installKeys();failAt=Names+63*0x80;LiveINI immediate;check(!lookup(immediate));failAt=0;check(lookup(immediate));check(immediate.indexDiagnostics().builds==2&&immediate.indexDiagnostics().failureReuses==0);
 // A root/header/generation replacement invalidates the failed bootstrap in
 // this same presentation, before its next-pass reset is needed.
 for(int variant=0;variant<5;++variant){initialize(64);installKeys();failAt=Names+63*0x80;LiveINI ini;ini.beginPresentation();check(!lookup(ini));failAt=0;
  if(variant==0){put(Object+0x28,Header{Entries,63,64});check(lookup(ini));}
  if(variant==1){put(Object+0x28,Header{Entries,64,65});check(lookup(ini));}
  if(variant==2){std::memcpy(memory.data()+0x110000,memory.data()+Entries,64*8);put(Object+0x28,Header{0x110000,64,64});check(lookup(ini));}
  if(variant==3)check(lookup(ini,Object,2));
  if(variant==4){const uint32_t other=Object+0x100;put(other,uint32_t(0x7E1AF4));put(other+0x28,Header{Entries,64,64});check(lookup(ini,other));}
  check(ini.indexDiagnostics().builds==2&&ini.indexDiagnostics().failureReuses==0);
 }
 initialize(64);installKeys();failAt=Names+63*0x80;LiveINI rootFailed;rootFailed.beginPresentation();check(!lookup(rootFailed));put(Object,uint32_t(0));failAt=0;check(!lookup(rootFailed));put(Object,uint32_t(0x7E1AF4));check(lookup(rootFailed));
 // A failed key/value lookup after a healthy bootstrap retains immediate
 // retry. The optimization does not memoize field failures or live values.
 initialize(64);installKeys();failAt=0x600000;LiveINI keyFailure;keyFailure.beginPresentation();check(!lookup(keyFailure));check(keyFailure.indexDiagnostics().builds==1&&keyFailure.indexDiagnostics().failures==0);failAt=0;check(lookup(keyFailure));
 // Opt-in cooldown bounds repeated full-index scans across render passes.
 initialize(64);installKeys();failAt=Names+63*0x80;LiveINI cooldown;cooldown.beginPresentation(1000,250);check(!lookup(cooldown));
 for(uint64_t t=1001;t<1250;t+=8){cooldown.beginPresentation(t,250);check(!lookup(cooldown));}
 check(cooldown.indexDiagnostics().builds==1&&cooldown.indexDiagnostics().failures==1);cooldown.beginPresentation(1250,250);check(!lookup(cooldown));check(cooldown.indexDiagnostics().builds==2);
 failAt=0;cooldown.beginPresentation(1499,250);check(!lookup(cooldown));cooldown.beginPresentation(1500,250);check(lookup(cooldown));check(cooldown.indexDiagnostics().builds==3);
 // Changed identity and explicit retirement bypass even an active cooldown.
 for(int variant=0;variant<3;++variant){initialize(64);installKeys();failAt=Names+63*0x80;LiveINI ini;ini.beginPresentation(1000,250);check(!lookup(ini));failAt=0;ini.beginPresentation(1001,250);
  if(variant==0){put(Object+0x28,Header{Entries,63,64});check(lookup(ini));}
  if(variant==1)check(lookup(ini,Object,2));
  if(variant==2){ini.clear();check(lookup(ini));}
  check(ini.indexDiagnostics().builds==2);
 }
 // Coalesced render root probes preserve exact fallback and invalidation.
 initialize(64);installKeys();failAt=Names+63*0x80;LiveINI batched;batched.beginPresentation(1000,250);check(!lookup(batched));
 calls=0;check(!lookup(batched));check(calls==1);
 failAt=Object+0x10;calls=0;check(!lookup(batched));check(calls==3); // unreadable gap, narrow fields remain readable
 failAt=0;put(Object+0x28,Header{Entries,63,64});check(lookup(batched));
 calls=0;check(lookup(batched));check(calls==2); // initial and final identity checks
 put(Object,uint32_t(0));check(!lookup(batched));put(Object,uint32_t(0x7E1AF4));check(lookup(batched));
 put(Object+0x28,Header{Entries,65,64});check(!lookup(batched));
 put(Object+0x28,Header{Entries,63,64});check(lookup(batched));
 failAt=Object+0x28;check(!lookup(batched));failAt=0;check(lookup(batched));
 initialize(64);installKeys();LiveINI narrow;narrow.beginPresentation();check(lookup(narrow));calls=0;check(lookup(narrow));check(calls==4);
 // Missing key/section and explicit-empty fallback rules stay unchanged.
 const char*disk="[SECTION0000]\nPALETTE=DISK\nMISSING=FILE\n[MISSINGSECTION]\nPALETTE=DISKONLY\n";std::vector<uint8_t>contents(disk,disk+std::strlen(disk));
 for(bool empty:{false,true}){initialize(64);installKeys(empty?" \t ":"  LIVE \r\n");BeforeEffectiveINI<Read> old(Read{},Object);EffectiveINI<Read> current(Read{},Object);old.parse(contents);current.parse(contents);old.generation(1);current.generation(1);current.beginPresentation();
  for(unsigned repeat=0;repeat<8;++repeat){check(old.get("SECTION0000","PALETTE")==current.get("SECTION0000","PALETTE"));check(current.get("SECTION0000","PALETTE")==std::string(empty?"":"LIVE"));check(old.get("SECTION0000","MISSING")==current.get("SECTION0000","MISSING"));check(current.get("SECTION0000","MISSING")=="FILE");check(old.get("MISSINGSECTION","PALETTE")==current.get("MISSINGSECTION","PALETTE"));check(current.get("MISSINGSECTION","PALETTE")=="DISKONLY");}
 }
 initialize(64);installKeys();failAt=Names+63*0x80;BeforeEffectiveINI<Read> old(Read{},Object);EffectiveINI<Read> current(Read{},Object);old.parse(contents);current.parse(contents);current.beginPresentation();check(old.get("SECTION0000","PALETTE")=="DISK"&&current.get("SECTION0000","PALETTE")=="DISK");failAt=0;current.beginPresentation();check(old.get("SECTION0000","PALETTE")=="LIVE"&&current.get("SECTION0000","PALETTE")=="LIVE");
 // Exercise the real consumer boundary, including cumulative diagnostics for
 // both independent INIs, rather than testing only the helper in isolation.
 initialize(64);installKeys();put(LiveINI::RulesPointer,Object);failAt=Names+63*0x80;
 Read reader;external::cameraassets::Context<Read> assets(reader);assets.beginPresentation(1000);
 for(unsigned n=0;n<2;++n){check(assets.artINI.get("SECTION0000","PALETTE").empty());check(assets.rulesINI.get("SECTION0000","PALETTE").empty());}
 auto stats=assets.liveINIDiagnostics();check(stats.builds==2&&stats.failures==2&&stats.failureReuses==2);
 failAt=0;assets.beginPresentation(1249);check(assets.artINI.get("SECTION0000","PALETTE").empty()&&assets.rulesINI.get("SECTION0000","PALETTE").empty());stats=assets.liveINIDiagnostics();check(stats.builds==2&&stats.failures==2&&stats.failureReuses==4);
 assets.beginPresentation(1250);check(assets.artINI.get("SECTION0000","PALETTE")=="LIVE"&&assets.rulesINI.get("SECTION0000","PALETTE")=="LIVE");stats=assets.liveINIDiagnostics();check(stats.builds==4&&stats.failures==2&&stats.failureReuses==4);
 // An absent binding must not resolve the same type and section again. A
 // genuine Image alias still inherits missing fields from the base section.
 initialize(64);installKeys();external::cameraassets::Context<Read> bindings(reader);bindings.metadata();bindings.beginPresentation(1000);
 const char*art="[DIRECT]\nPalette=DIRECTPAL\n[ORIGINAL]\nImage=ALIAS\nPalette=BASEPAL\nTerrainPalette=yes\n[ALIAS]\nPalette=ALIASPAL\n";
 bindings.artINI.parse(std::vector<uint8_t>(art,art+std::strlen(art)));
 stringAt(Type+0x1F8,"DIRECT");typeNameReads=0;
 check(bindings.artValue(Type,"missing").empty()&&typeNameReads==1);
 check(bindings.artValue(Type,"palette")=="DIRECTPAL");
 stringAt(Type+0x1F8,"ORIGINAL");typeNameReads=0;
 auto sections=bindings.artSections(Type);
 check(sections.image=="ALIAS"&&bindings.artValue(sections,"palette")=="ALIASPAL"&&bindings.artYes(sections,"terrainpalette")&&typeNameReads==1);
 std::printf("PASS %u active assertions: exact fallback/empty semantics, render cooldown, root/header/generation invalidation, key retry and shared asset binding resolution. No game or GPU execution.\n",checks);
}
