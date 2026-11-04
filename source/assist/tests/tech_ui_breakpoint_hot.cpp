// Production signature snapshot and owner-counter checks in synthetic memory.
// Timings measure only signature validation, never Windows DR/VEH or game FPS.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <initializer_list>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <chrono>
#endif
#include "../source/tech_ui_breakpoint_hot.h"

#ifdef NDEBUG
#error Hot-path verification requires assertions enabled.
#endif
#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

namespace {
unsigned checks=0;
void check(bool okay){if(!okay){fprintf(stderr,"hot-path check %u failed\n",checks+1);abort();}++checks;}
constexpr size_t ByteCount=20;
constexpr uint32_t TestAddress=0x10000;
uint8_t bytes[ByteCount]{};
bool readable=true;
uint32_t hash(const uint8_t*data,size_t size){uint32_t h=2166136261u;for(size_t i=0;i<size;++i)h=(h^data[i])*16777619u;return h;}
bool reader(uint32_t at,void*out,size_t size){
 if(!readable||at!=TestAddress||size!=sizeof bytes)return false;
 for(size_t i=0;i<size;++i)static_cast<uint8_t*>(out)[i]=bytes[i];return true;
}
bool oldRead(const volatile uint8_t*data,uint8_t*out,size_t size){
#ifdef _WIN32
 SIZE_T count=0;return ReadProcessMemory(GetCurrentProcess(),const_cast<const uint8_t*>(data),out,size,&count)&&count==size;
#else
 for(size_t i=0;i<size;++i)out[i]=data[i];return true;
#endif
}
NOINLINE bool oldSignature(const volatile uint8_t*data,uint32_t expected){
 uint8_t copy[ByteCount]{};return oldRead(data,copy,sizeof copy)&&hash(copy,sizeof copy)==expected;
}
using Snapshot=assist::techbp::SignatureSnapshot<ByteCount>;
NOINLINE bool newSignature(const Snapshot&snapshot,const volatile uint8_t*data){return snapshot.matches(data);}
// Exercise the dispatch helper against the Lease::actualTag contract without
// importing Win32 CONTEXT or making any operating-system debug-register writes.
struct TestRegisters {uint32_t address[4]{},control=0,status=0;};
struct TestLease {
 uint32_t sites[5]{0x11000,0x12000,0x13000,0x14000,0x15000};
 uint32_t mask=1,uncertain=0;uint8_t tag[4]{},alternative[4]{};
 unsigned actualTag(const TestRegisters&r,unsigned n)const{
  const uint32_t slotMask=(3u<<(n*2))|(15u<<(16+n*4));
  if(!(mask&(1u<<n))||(r.control&slotMask)!=(1u<<(n*2)))return 5;
  if(r.address[n]==sites[tag[n]])return tag[n];
  return (uncertain&(1u<<n))&&r.address[n]==sites[alternative[n]]?alternative[n]:5;
 }
};
void hitChecks(){
 TestLease lease{};TestRegisters registers{};registers.address[0]=lease.sites[0];registers.control=1;registers.status=1;
 auto hit=[&](uint32_t eip){const TestLease savedLease=lease;const TestRegisters savedRegisters=registers;auto result=assist::techbp::ownedHit(lease,registers,eip,5);check(!memcmp(&lease,&savedLease,sizeof lease)&&!memcmp(&registers,&savedRegisters,sizeof registers));return result;};
 auto result=hit(lease.sites[0]);check(result.slots==1&&result.tag==0);
 registers.status=0;check(!hit(lease.sites[0]).slots);registers.status=1;
 for(uint32_t foreign:{2u,4u,8u,0x2000u,0x4000u,0x8000u}){registers.status=1|foreign;check(!hit(lease.sites[0]).slots);}registers.status=1;
 check(!hit(lease.sites[0]+1).slots);
 // Global enable and each non-execution RW/LEN configuration are foreign.
 for(uint32_t control:{0u,2u,3u,0x10001u,0x20001u,0x40001u,0x80001u}){registers.control=control;check(!hit(lease.sites[0]).slots);}registers.control=1;
 registers.address[0]=lease.sites[4];check(!hit(lease.sites[4]).slots);
 lease.uncertain=1;lease.alternative[0]=4;result=hit(lease.sites[4]);check(result.slots==1&&result.tag==4);
 check(!hit(lease.sites[0]).slots);lease.uncertain=0;registers.address[0]=lease.sites[0];
 lease.mask=0;check(!hit(lease.sites[0]).slots);lease.mask=1;
 // A second triggered slot with an invalid address must leave the first one
 // unhandled too, so another handler receives the complete mixed exception.
 lease.mask=3;lease.tag[1]=1;registers.control=5;registers.status=3;registers.address[1]=lease.sites[1];check(!hit(lease.sites[0]).slots);
}
double nowSeconds(){
#ifdef _WIN32
 LARGE_INTEGER value{},frequency{};if(!QueryPerformanceCounter(&value)||!QueryPerformanceFrequency(&frequency))abort();
 return double(value.QuadPart)/double(frequency.QuadPart);
#else
 return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}
struct Timing{double oldUs,newUs;uint32_t oldAccepted,newAccepted;};
Timing benchmark(const Snapshot&snapshot,uint32_t expected,unsigned count,bool oldFirst){
 Timing t{};
 auto oldRun=[&](){uint32_t accepted=0;const double start=nowSeconds();for(unsigned n=0;n<count;++n)accepted+=oldSignature(bytes,expected);t.oldUs=(nowSeconds()-start)*1000000.0;t.oldAccepted=accepted;};
 auto newRun=[&](){uint32_t accepted=0;const double start=nowSeconds();for(unsigned n=0;n<count;++n)accepted+=newSignature(snapshot,bytes);t.newUs=(nowSeconds()-start)*1000000.0;t.newAccepted=accepted;};
 if(oldFirst){oldRun();newRun();}else{newRun();oldRun();}return t;
}
}

int main(){
 hitChecks();
 for(size_t i=0;i<sizeof bytes;++i)bytes[i]=uint8_t((i*37u+11u)&255u);
 const uint32_t expected=hash(bytes,sizeof bytes);Snapshot snapshot{};
 check(!snapshot.matches(bytes));check(snapshot.refresh(TestAddress,expected,reader));check(snapshot.matches(bytes));
 for(size_t i=0;i<sizeof bytes;++i){bytes[i]^=0x80u;check(!snapshot.matches(bytes));bytes[i]^=0x80u;check(snapshot.matches(bytes));}
 check(!snapshot.refresh(TestAddress,expected^1u,reader));check(!snapshot.matches(bytes));
 check(snapshot.refresh(TestAddress,expected,reader));readable=false;
 check(!snapshot.refresh(TestAddress,expected,reader));check(!snapshot.matches(bytes));readable=true;
 check(!snapshot.refresh(TestAddress+1,expected,reader));check(!snapshot.matches(bytes));
 check(snapshot.refresh(TestAddress,expected,reader));check(snapshot.matches(bytes));
 volatile int32_t signedCounter=INT32_MAX;assist::techbp::countOwner(signedCounter);check(uint32_t(signedCounter)==0x80000000u);
 signedCounter=-1;assist::techbp::countOwner(signedCounter);check(signedCounter==0);
 volatile uint32_t unsignedCounter=UINT32_MAX;assist::techbp::countOwner(unsignedCounter);check(unsignedCounter==0);
 unsignedCounter=41;assist::techbp::countOwner(unsignedCounter);check(unsignedCounter==42);
 constexpr unsigned Iterations=100000,Rounds=3;Timing times[Rounds]{};
 // Warm both paths before timing; correctness checks stay outside measured loops.
 for(unsigned n=0;n<1000;++n){check(oldSignature(bytes,expected));check(newSignature(snapshot,bytes));}
 for(unsigned n=0;n<Rounds;++n){times[n]=benchmark(snapshot,expected,Iterations,(n&1)==0);check(times[n].oldAccepted==Iterations&&times[n].newAccepted==Iterations);}
#ifdef _WIN32
 const char*baseline="ReadProcessMemory(GetCurrentProcess)+FNV";
#else
 const char*baseline="volatile memory reader+FNV; no Windows syscall";
#endif
 printf("{\"pass\":true,\"checks\":%u,\"scope\":\"production signature helper and owner counter on synthetic own-process bytes\",\"baseline\":\"%s\",\"signature_bytes\":%zu,\"iterations_per_round\":%u,\"windows_veh_measured\":false,\"game_process_tested\":false,\"fps_measured\":false,\"runs\":[",checks,baseline,ByteCount,Iterations);
 for(unsigned n=0;n<Rounds;++n)printf("%s{\"old_total_us\":%.6f,\"new_total_us\":%.6f,\"old_us_per_check\":%.9f,\"new_us_per_check\":%.9f,\"accepted_each\":%u}",n?",":"",times[n].oldUs,times[n].newUs,times[n].oldUs/Iterations,times[n].newUs/Iterations,times[n].oldAccepted);
 puts("]}");
}
