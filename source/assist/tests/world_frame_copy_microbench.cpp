// Synthetic Win32 copy-throughput benchmark; NOT a gameplay FPS claim.
// Compare contiguous 3.5 MiB memcpy against read calls to current process.
// Never use timing as pass/fail: CI VMs have variable load.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
int main(){
 constexpr size_t Page=4096, Bytes=896*Page, Passes=64;
 std::vector<uint8_t> src(Bytes),dst(Bytes,0);
 for(size_t i=0;i<Bytes;++i)src[i]=uint8_t(i*13u+(i>>12));
 volatile uint64_t check=0;
 struct Result{double ms=0;size_t calls=0;};
 const auto measure=[&](size_t chunk,bool local)->Result{
  Result result{};
  auto run=[&](bool count)->bool{
   for(size_t i=0;i<Bytes;i+=chunk){
    const size_t n=std::min(chunk,Bytes-i);
    if(local)std::memcpy(dst.data()+i,src.data()+i,n);
    else{
     SIZE_T got=0;
     if(!ReadProcessMemory(GetCurrentProcess(),src.data()+i,dst.data()+i,n,&got)||
        got!=n)return false;
    }
    if(count)++result.calls;
   }
   // Consume output to prevent eliminating memcpy in optimized builds.
   check+=dst[0]+dst[Bytes-1]+dst[Bytes/2];
   return true;
  };
  for(unsigned warm=0;warm<4;++warm)assert(run(false));
  const auto start=std::chrono::steady_clock::now();
  for(size_t sample=0;sample<Passes;++sample)assert(run(true));
  const auto stop=std::chrono::steady_clock::now();
  result.ms=std::chrono::duration<double,std::milli>(stop-start).count()/Passes;
  assert(dst==src); // Validate outside timed loop, not 64 extra full scans.
  return result;
 };
 const Result copy=measure(Bytes,true);
 const Result rpm64=measure(16*Page,false);
 const Result rpm128=measure(32*Page,false);
 assert(copy.calls==Passes);
 assert(rpm64.calls==Passes*56&&rpm128.calls==Passes*28);
 assert(rpm64.calls==2*rpm128.calls);
 std::printf(
  "PASS native copy microbench (synthetic current-process): bytes=%zu passes=%zu "
  "memcpy_ms_per_copy=%.5f memcpy_calls_per_copy=1 "
  "rpm64_ms_per_copy=%.5f rpm64_calls_per_copy=%zu "
  "rpm128_ms_per_copy=%.5f rpm128_calls_per_copy=%zu "
  "checksum=%llu real_game=false\\n",
  Bytes,Passes,copy.ms,rpm64.ms,rpm64.calls/Passes,rpm128.ms,
  rpm128.calls/Passes,(unsigned long long)check);
}
