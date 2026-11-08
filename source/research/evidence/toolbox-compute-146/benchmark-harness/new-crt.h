#include <cstring>
#include <cstdint>
#include "/workspace/scratch/40121bc881b8/ra2-audit-followup/source/shared/source/runtime_memory.h"
extern "C" void* memset(void*p,int c,size_t n)noexcept{return assist::runtimememory::fill(p,c,n);}
extern "C" void* memcpy(void*p,const void*q,size_t n)noexcept{return assist::runtimememory::copy(p,q,n);}
