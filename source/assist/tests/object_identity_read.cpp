#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "object_index.h"
static assist::ObjectIndex worldObjects;
static std::vector<uint8_t>live(0x300000),sealed(0x300000);
static unsigned sampleReadDepth=0,checks=0,digest=2166136261u;
static unsigned rpm=0,localReads=0,sampleReads=0;
static uint32_t unreadable=0;
static bool sampleComplete=true,sampleMissing=false;
static bool read(uint32_t address,void*out,size_t n){
 if(sampleReadDepth){++sampleReads;if(sampleMissing){sampleComplete=false;return false;}}
 else ++rpm;
 if(address<0x10000||address==unreadable||uint64_t(address)+n>=live.size())return false;
 std::memcpy(out,(sampleReadDepth?sealed:live).data()+address,n);return true;
}
extern "C" int GuardedReadObjectIdentity(unsigned object,unsigned*out){
 ++localReads;assert(!sampleReadDepth);
 if(!out||object<0x10000||uint64_t(object)+0x14>=live.size()||object+0x10==unreadable)return 0;
 std::memcpy(out,live.data()+object+0x10,4);return 1;
}
#include "identity_body.inc"
static void check(bool ok){++checks;assert(ok);digest=(digest^unsigned(ok))*16777619u;}
static void id(uint32_t object,uint32_t value){std::memcpy(live.data()+object+0x10,&value,4);}
int main(){
 constexpr uint32_t p=0x100000,q=p+0x1000;
 worldObjects.add(p,77);id(p,77);id(q,88);sealed=live;
 check(worldByID(77)==p&&rawObjectID(p)==77&&!rawObjectID(q)&&!worldByID(88));
 id(p,99);check(!worldByID(77)&&rawObjectID(p)==99); // same pointer, changed live ID
 sampleReadDepth=1;check(worldByID(77)==p&&rawObjectID(p)==77);
 sampleReadDepth=2;check(worldByID(77)==p);
 const auto localBefore=localReads,rpmBefore=rpm;
 sampleMissing=true;check(!worldByID(77)&&!rawObjectID(p)&&!sampleComplete);
 check(localReads==localBefore&&rpm==rpmBefore); // no live fallback on a missing page
 sampleMissing=false;sampleReadDepth=0;
 unreadable=p+0x10;check(!worldByID(77)&&!rawObjectID(p));unreadable=0;
 for(unsigned n=0;n<500;++n){id(p,n%2?77:99);check((worldByID(77)==p)==bool(n%2));}
 id(p,77);rpm=localReads=sampleReads=0;
 for(unsigned n=0;n<100000;++n){assert(worldByID(77)==p);assert(rawObjectID(p)==77);}
 const auto observedRpm=rpm,observedLocal=localReads;
#ifdef LOCAL_IDENTITY
 check(rpm==0&&localReads==200000);
#else
 check(rpm==200000&&localReads==0);
#endif
 worldObjects.clear();check(!worldByID(77)&&!rawObjectID(p));
 check(rpm==observedRpm&&localReads==observedLocal);
 std::printf("{\"pass\":true,\"checks\":%u,\"semantic_digest\":%u,\"rpm_calls\":%u,\"guarded_local_reads\":%u,\"identity_reads\":200000}\n",checks,digest,observedRpm,observedLocal);
}
