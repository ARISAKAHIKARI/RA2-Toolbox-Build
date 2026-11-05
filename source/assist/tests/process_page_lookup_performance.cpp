#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <vector>
#ifndef RA2_PROCESS_READER_HEADER
#define RA2_PROCESS_READER_HEADER "../../external_reader/source/process_reader.h"
#endif
#include RA2_PROCESS_READER_HEADER

int main(){
 constexpr size_t Pages=2048,Bytes=Pages*4096,Fields=24,Passes=4;
 void*memory=nullptr;
 for(uintptr_t base=0x10000000;base<0x60000000&&!memory;base+=0x04000000)
  memory=VirtualAlloc(reinterpret_cast<void*>(base),Bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 assert(memory&&uintptr_t(memory)+Bytes<0x80000000ull);
 const auto address=uint32_t(uintptr_t(memory));
 for(size_t page=0;page<Pages;++page)for(size_t field=0;field<Fields;++field)
  reinterpret_cast<uint32_t*>(static_cast<unsigned char*>(memory)+page*4096)[field]=uint32_t(page*31+field);
 external::ProcessReader reader;assert(reader.open(GetCurrentProcessId()));
 std::vector<uint32_t>addresses;for(size_t page=0;page<Pages;++page)addresses.push_back(address+uint32_t(page*4096));
 reader.prefetch(addresses,Fields*4);assert(reader.cachedPageStorage()==Pages);
 uint64_t checksum=0;std::vector<double>times;
 for(unsigned round=0;round<5;++round){
  const auto started=std::chrono::steady_clock::now();
  for(size_t pass=0;pass<Passes;++pass)for(size_t field=0;field<Fields;++field)for(size_t p=0;p<Pages;++p){
   const size_t page=(p*197+pass*17)%Pages;uint32_t value=0;
   assert(reader.read(address+uint32_t(page*4096+field*4),&value,4));
   assert(value==page*31+field);checksum+=value;
  }
  times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
 }
 assert(reader.pageHits==5*Passes*Fields*Pages&&reader.reads==Pages/16);
 // Values are retained only within one observation, and the next epoch is fresh.
 *static_cast<uint32_t*>(memory)=0x76543210;uint32_t value=0;
 assert(reader.read(address,&value,4)&&value==0);reader.begin();
 assert(reader.read(address,&value,4)&&value==0x76543210&&reader.reads==1);
 assert(reader.pageBufferAllocations()==Pages);
 std::printf("{\"pages\":%zu,\"fields\":%zu,\"passes\":%zu,\"lookups_per_round\":%zu,\"checksum\":%llu,\"milliseconds\":[",Pages,Fields,Passes,Pages*Fields*Passes,(unsigned long long)checksum);
 for(size_t i=0;i<times.size();++i)std::printf("%s%.6f",i?",":"",times[i]);
 std::puts("],\"byte_and_epoch_checks\":true}");
 reader.close();assert(VirtualFree(memory,0,MEM_RELEASE));
}
