#pragma once
#include "native_entry_policy.h"

static nativeentry::TimedResult nativeAresCache,nativeEntryCache[3];
static bool nativeExecutableRange(uint32_t address,size_t bytes){
 if(!nativeentry::range(address,bytes))return false;
 MEMORY_BASIC_INFORMATION region{};
 return VirtualQuery((void*)(uintptr_t)address,&region,sizeof region)==sizeof region&&
  region.State==MEM_COMMIT&&(region.Type==MEM_IMAGE||region.Type==MEM_PRIVATE)&&
  !(region.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&
  (region.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))&&
  uintptr_t(region.BaseAddress)<=address&&
  uint64_t(address)+bytes<=uint64_t(uintptr_t(region.BaseAddress))+region.RegionSize;
}
static bool nativeAresAvailable(bool refresh=false){
 const DWORD now=GetTickCount();if(!refresh&&nativeAresCache.fresh(now))return nativeAresCache.value;
 const uint32_t module=uint32_t(uintptr_t(GetModuleHandleW(L"Ares.dll")));
 bool valid=false;IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS32 nt{};MEMORY_BASIC_INFORMATION region{};
 if(nativeentry::range(module,sizeof dos)&&read(module,&dos,sizeof dos)&&dos.e_magic==IMAGE_DOS_SIGNATURE&&dos.e_lfanew>=64&&dos.e_lfanew<=4096&&
    read(module+uint32_t(dos.e_lfanew),&nt,sizeof nt)&&nt.Signature==IMAGE_NT_SIGNATURE&&nt.FileHeader.Machine==IMAGE_FILE_MACHINE_I386&&
    nt.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC&&nt.OptionalHeader.SizeOfImage>=4096&&nt.OptionalHeader.SizeOfImage<=0x10000000&&
    uint64_t(module)+nt.OptionalHeader.SizeOfImage<0x80000000ull&&
    VirtualQuery((void*)(uintptr_t)module,&region,sizeof region)==sizeof region&&region.State==MEM_COMMIT&&region.Type==MEM_IMAGE&&
    uintptr_t(region.AllocationBase)==module&&!(region.Protect&(PAGE_GUARD|PAGE_NOACCESS)))valid=true;
 return nativeAresCache.set(now,valid);
}
static bool nativeEntryCallable(uint32_t address,bool refresh=false){
 const int slot=nativeentry::slot(address);if(slot<0)return false;
 const DWORD now=GetTickCount();auto&cache=nativeEntryCache[slot];if(!refresh&&cache.fresh(now))return cache.value;
 const Signature*signature=nullptr;for(const auto&s:Signatures)if(s.address==address){signature=&s;break;}
 uint8_t bytes[12]{};bool valid=false;
 if(signature&&signature->length<=sizeof bytes&&nativeExecutableRange(address,signature->length)&&read(address,bytes,signature->length)){
  if(!memcmp(bytes,signature->bytes,signature->length))valid=true;
  else if(nativeAresAvailable(refresh))valid=nativeentry::platformCallable(address,true,read,nativeExecutableRange);
 }
 return cache.set(now,valid);
}
