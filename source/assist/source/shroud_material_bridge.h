#include "shroud_material.h"
#include "shroud_policy.h"
static shroudmaterials::Controller shroudMaterials;
static bool writeShroudMaterial(uint32_t address,const void*data,size_t bytes){
 if(!gameentry::active(gameentry::Client::Assist)||address<0x10000||!bytes||bytes>128*1024||uint64_t(address)+bytes>0x80000000ull)return false;
 // No executable section/page or protection changes. SHP data stays owned
 // and freed by the engine's original resource allocation.
 for(uint64_t at=address,end=uint64_t(address)+bytes;at<end;){MEMORY_BASIC_INFORMATION region{};
  if(VirtualQuery((const void*)(uintptr_t)at,&region,sizeof region)!=sizeof region||region.State!=MEM_COMMIT||(region.Protect&(PAGE_GUARD|PAGE_NOACCESS))||
     (region.Protect&0xff)!=PAGE_READWRITE)return false;
  uint64_t next=uint64_t(uintptr_t(region.BaseAddress))+region.RegionSize;if(next<=at)return false;at=std::min(next,end);
 }
 SIZE_T written=0;return WriteProcessMemory(GetCurrentProcess(),(void*)(uintptr_t)address,data,bytes,&written)&&written==bytes;
}
