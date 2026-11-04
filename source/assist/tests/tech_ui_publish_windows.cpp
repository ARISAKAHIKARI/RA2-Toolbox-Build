#ifdef NDEBUG
#error Test checks must be active
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
static bool failRemoval=false;static unsigned rejectedRemovals=0;
static ULONG testRemoveVeh(PVOID handler){if(failRemoval){++rejectedRemovals;SetLastError(ERROR_INVALID_HANDLE);return 0;}return RemoveVectoredExceptionHandler(handler);}
#define RemoveVectoredExceptionHandler testRemoveVeh
#include TECH_CORE_HEADER
#include "../source/tech_enhancement_parameter_policy.h"
#undef RemoveVectoredExceptionHandler
struct Shared {uint32_t hardwareSlotBudget=0;int techParameterStatus=0;unsigned techParameterError=0,techParameterSlots=0,techParameterExceptions=0;int techEnhanceParameterStatus=0;unsigned techEnhanceParameterError=0,techEnhanceParameterSlots=0,techEnhanceParameterExceptions=0;}wire,*sh=&wire;
namespace techparameters {
inline constexpr uint32_t FullGroup=1u;
static assist::techbp::Service service;static bool desired=false,enhancementDesired=false,enhancementCompareRequested=true;static int failure=0,enhancementFailure=0;static unsigned refusedGroups=0,resourceGroups=0;static volatile LONG fullExceptions=0,enhancementExceptions=0;
namespace techbp=assist::techbp;namespace techenhanceparams=assist::techenhanceparams;
#include TECH_PUBLISH_ROUTES
}
static unsigned checks=0;static void check(bool okay){++checks;if(!okay){std::fprintf(stderr,"check %u failed\n",checks);std::abort();}}
static volatile LONG dummy=0;
extern "C" __declspec(noinline) void site0(){InterlockedIncrement(&dummy);}
extern "C" __declspec(noinline) void site1(){InterlockedDecrement(&dummy);}
extern "C" __declspec(noinline) void site2(){InterlockedExchangeAdd(&dummy,2);}
extern "C" __declspec(noinline) void site3(){InterlockedExchangeAdd(&dummy,3);}
static bool callback(void*,uint32_t){return false;}
static void settle(){DWORD before=GetTickCount();while(techparameters::service.pending()&&DWORD(GetTickCount()-before)<5000)Sleep(1);check(!techparameters::service.pending());}
int main(){using namespace techparameters;const uint32_t sites[]={uint32_t(uintptr_t(&site0)),uint32_t(uintptr_t(&site1)),uint32_t(uintptr_t(&site2)),uint32_t(uintptr_t(&site3))};
 check(service.configure(sites,12,callback));settle();check(service.activeTags()==12&&service.resources()&&service.state==techbp::Ready);enhancementDesired=true;resourceGroups=12;publish();check(sh->techEnhanceParameterStatus==techbp::Ready&&sh->techParameterStatus==0);
 enhancementDesired=false;check(!service.retire());settle();check(service.activeTags()==0&&!service.lease.mask);failRemoval=true;check(!service.retire());check(service.state==techbp::System&&service.resources()&&!service.pending());publish();const int failedStatus=sh->techEnhanceParameterStatus;
#if EXPECT_HIDDEN
 check(failedStatus==techbp::Off); // Expected counterexample: ownership remains but UI says Off.
#else
 check(failedStatus==techbp::System);check(sh->techParameterStatus==0); // Never requested Full must remain Off.
#endif
 check(rejectedRemovals==1&&sh->techEnhanceParameterError==ERROR_INVALID_HANDLE);failRemoval=false;check(service.retire());check(!service.resources());publish();check(sh->techEnhanceParameterStatus==0&&sh->techParameterStatus==0);
 std::printf("{\"pass\":true,\"checks\":%u,\"before_fix_expected\":%s,\"failed_remove_status\":%d,\"remove_failures\":%u,\"real_own_process_DR_used\":true,\"game_process_accessed\":false,\"assertions_enabled\":true}\n",checks,EXPECT_HIDDEN?"true":"false",failedStatus,rejectedRemovals);
}
