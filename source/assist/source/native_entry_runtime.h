#pragma once
#include "native_entry_policy.h"
#include "native_platform_profile.h"

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

// Resolve ownership from the callback's actual mapped image, not a DLL name.
// The profile independently verifies PE bounds, declaration and named export.
static uint32_t nativeImageAllocation(uint32_t address){
 MEMORY_BASIC_INFORMATION region{};
 if(!nativeentry::range(address,1)||VirtualQuery((void*)(uintptr_t)address,&region,sizeof region)!=sizeof region||
    region.State!=MEM_COMMIT||region.Type!=MEM_IMAGE||(region.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return 0;
 const uintptr_t base=uintptr_t(region.AllocationBase);
 return base>=0x10000&&base<0x80000000ull?uint32_t(base):0;
}
static bool nativeImageOwned(uint32_t module,uint32_t address,size_t bytes,bool code){
 if(!nativeentry::range(module,1)||!nativeentry::range(address,bytes))return false;
 MEMORY_BASIC_INFORMATION region{};
 return VirtualQuery((void*)(uintptr_t)address,&region,sizeof region)==sizeof region&&
  region.State==MEM_COMMIT&&region.Type==MEM_IMAGE&&uintptr_t(region.AllocationBase)==module&&
  !(region.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(!code||
   (region.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))&&
  uintptr_t(region.BaseAddress)<=address&&uint64_t(address)+bytes<=uint64_t(uintptr_t(region.BaseAddress))+region.RegionSize;
}
static nativeplatform::Inspection nativeOrdinaryInspections[3];
static bool nativeOrdinaryEntryKnown(uint32_t address){return nativeentry::known(address)||nativeplatform::known(address);}
static bool nativeOrdinaryEntryCallable(uint32_t address,bool refresh=false){
 const int slot=nativeplatform::slot(address);
 if(slot>=0){nativeOrdinaryInspections[slot]={};nativeOrdinaryInspections[slot].entry=address;}
 // Preserve the previously supported Ares path. Raw DR callers never enter
 // this wrapper and therefore never inherit the new Phobos-only evidence.
 if(nativeentry::known(address)&&nativeEntryCallable(address,refresh))return true;
 if(slot<0)return false;
 return nativeplatform::inspect(address,nativeOrdinaryInspections[slot],read,nativeExecutableRange,nativeImageAllocation,nativeImageOwned);
}
static void nativeOrdinaryReport(uint32_t address,bool accepted,bool inspected){
 const int slot=nativeplatform::slot(address);if(slot<0)return;
 auto*observation=debugentries::observation(debugentries::Kind::Call,address);if(!observation)return;
 const bool oldProfileError=observation->error<-nativeplatform::ErrorBase&&
  observation->error>=-nativeplatform::ErrorBase-int(nativeplatform::Failure::Changed);
 if(!inspected)nativeOrdinaryInspections[slot]={};
 if(accepted){if(oldProfileError)debugentries::installation(debugentries::Kind::Call,address,debugentries::Hook::NA);return;}
 if(!inspected){if(oldProfileError)observation->error=0;return;}
 const auto&result=nativeOrdinaryInspections[slot];if(result.entry!=address||result.failure==nativeplatform::Failure::None)return;
 using Failure=nativeplatform::Failure;debugentries::Reason reason=debugentries::Reason::Structural;
 if(result.failure==Failure::EntryUnreadable||result.failure==Failure::StubUnreadable)reason=debugentries::Reason::Unreadable;
 else if(result.failure==Failure::ModuleIdentity||result.failure==Failure::HookDeclaration||result.failure==Failure::CallbackOwner)reason=debugentries::Reason::Foreign;
 else if(result.failure==Failure::Capabilities||result.failure==Failure::UnknownCallbackChain)reason=debugentries::Reason::Unsupported;
 debugentries::installation(debugentries::Kind::Call,address,debugentries::Hook::NA,reason,-nativeplatform::ErrorBase-int(result.failure));
}
