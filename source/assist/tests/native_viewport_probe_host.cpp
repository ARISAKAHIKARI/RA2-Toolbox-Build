#ifdef NDEBUG
#undef NDEBUG
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include "../source/protocol.h"
#include "../source/native_viewport_host.h"
#ifdef NDEBUG
#error "native viewport probe assertions must be active"
#endif
using namespace assist;
namespace nv=nativeviewhost;
static DWORD pid=0,openFailure=0;static Shared*shared=nullptr,*mapped=nullptr;static HANDLE mapping=nullptr;
static unsigned opens=0,checks=0;static uint64_t created=0;
static HANDLE probeOpen(DWORD access,BOOL inherit,const wchar_t*name){
 ++opens;assert(access==FILE_MAP_READ&&!inherit);
 if(openFailure){SetLastError(openFailure);return nullptr;}
 return OpenFileMappingW(access,inherit,name);
}
#define OpenFileMappingW probeOpen
#include VIEWPORT_PROBE_ROUTES
#undef OpenFileMappingW
static void check(bool yes){assert(yes);++checks;}
static void closeMapping(){if(mapped){check(UnmapViewOfFile(mapped)!=0);mapped=nullptr;}if(mapping){check(CloseHandle(mapping)!=0);mapping=nullptr;}}
static void fresh(){
 closeMapping();shared=nullptr;openFailure=0;opens=0;
 nv::lifecycle()=nv::Lifecycle{};nv::capabilityLease()={};nv::bindProcess(pid,created);
}
static void newMapping(size_t bytes=sizeof(Shared)){
 wchar_t name[100];mappingName(name,pid);mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,DWORD(bytes),name);
 check(mapping&&GetLastError()!=ERROR_ALREADY_EXISTS);if(bytes!=sizeof(Shared))return;
 mapped=static_cast<Shared*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));check(mapped!=nullptr);
 memset(mapped,0,sizeof *mapped);mapped->magic=Magic;mapped->version=Version;mapped->pid=pid;
}
static void publication(bool active,uint32_t generation=1,uint32_t serial=2){
 assert(mapped);mapped->viewportSerial=LONG(serial);mapped->viewportGeneration=generation;mapped->viewportActive=active;
 const int a[]={168,16,632,568},r[]={0,0,948,852};
 for(unsigned i=0;i<4;++i){mapped->viewportPhysical[i]=a[i];mapped->viewportLogical[i]=r[i];}
 mapped->viewportHostCapabilities=nativeviewportprojection::HostHandshake|(active?3u:0u);
}
static bool identity(){nv::Transform out;out.active=true;return nv::sampleTransform(static_cast<const Shared*>(nullptr),out)&&!out.active;}
static void expire(){check(nv::probeAbsent(nv::processIdentity(),nv::probeClock()-nv::Lifecycle::ProbeInterval-1));check(nv::probeDue());}
int main(){
 pid=GetCurrentProcessId();FILETIME c{},e{},k{},u{};check(GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u)!=0);created=(uint64_t(c.dwHighDateTime)<<32)|c.dwLowDateTime;
 // First frame probes before any identity projection. Only a truly absent
 // mapping may permit ordinary read-only legacy display without Runtime.
 fresh();check(nv::probeDue()&&!identity());probeNativeViewportMapping();check(opens==1&&identity()&&!nv::probeDue());
 probeNativeViewportMapping();check(opens==1);expire();check(!identity());probeNativeViewportMapping();check(opens==2&&identity());
 // Access denied and wrong-size views are unknown, never mapping absence.
 fresh();openFailure=ERROR_ACCESS_DENIED;probeNativeViewportMapping();check(opens==1&&nv::lifecycle().unknown()&&!identity());
 openFailure=0;expire();probeNativeViewportMapping();check(nv::lifecycle().unknown()&&!identity());
 fresh();newMapping(4096);probeNativeViewportMapping();check(nv::lifecycle().unknown()&&!identity());
 // Real named mappings exercise the exact production Open/Map/read/Unmap
 // sequence. Invalid protocol/PID cannot assert that private R is retired.
 for(unsigned bad=0;bad<3;++bad){fresh();newMapping();publication(false);if(bad==0)mapped->magic=0;if(bad==1)++mapped->version;if(bad==2)++mapped->pid;
  probeNativeViewportMapping();check(opens==1&&nv::lifecycle().unknown()&&!identity());}
 for(unsigned bad=0;bad<4;++bad){fresh();newMapping();publication(false);if(bad==0)mapped->viewportSerial=0;if(bad==1)mapped->viewportGeneration=0;if(bad==2)mapped->viewportSerial=3;if(bad==3)mapped->viewZoomBusy=1;
  probeNativeViewportMapping();check(nv::lifecycle().unknown()&&!identity());}
 fresh();newMapping();publication(false);probeNativeViewportMapping();check(opens==1&&identity()&&!nv::lifecycle().unknown());
 // Existing active R from another/crashed Host blocks this Host's FIRST
 // read-only frame. Same PID/creation rebind must not remove that latch.
 fresh();newMapping();publication(true);probeNativeViewportMapping();check(opens==1&&nv::lifecycle().unknown()&&!identity());
 nv::bindProcess(pid,created);check(!identity()&&!nv::probeDue());probeNativeViewportMapping();check(opens==1&&!identity());
 publication(false,2,4);expire();probeNativeViewportMapping();check(opens==2&&identity()&&!nv::lifecycle().unknown());
 // The actual ensureBridge ERROR_ALREADY_EXISTS prefix observes the prior
 // mapping before stop/unmap, including a never-painted active publication.
 fresh();newMapping();publication(true);mapped->stop=0;const auto priorCaps=mapped->viewportHostCapabilities;
 check(fixturePriorPrefix());check(nv::lifecycle().unknown()&&!identity());check(mapped->stop==0&&mapped->viewportHostCapabilities==priorCaps);
 closeMapping();expire();probeNativeViewportMapping();check(nv::lifecycle().unknown()&&!identity());
 newMapping();probeNativeViewportMapping();check(!identity());expire();probeNativeViewportMapping();check(nv::lifecycle().unknown()&&!identity());
 publication(false,3,6);expire();probeNativeViewportMapping();check(identity());
 fresh();newMapping();publication(false);mapped->version=0;check(!fixturePriorPrefix());check(nv::lifecycle().unknown()&&!identity());
 // A changed attested process resets to probe-pending, not immediate
 // identity; an old process's callback cannot authorize its successor.
 fresh();const auto old=nv::processIdentity();nv::bindProcess(pid,created+1);check(!identity()&&nv::probeDue());
 check(!nv::probeAbsent(old)&&!identity());probeNativeViewportMapping();check(identity());
 // An owned Shared lease is sampled directly elsewhere; this metadata
 // wrapper must not replace it with a second mapping's classification.
 fresh();newMapping();publication(true);shared=mapped;probeNativeViewportMapping();check(opens==0);shared=nullptr;probeNativeViewportMapping();check(opens==1&&!identity());
 closeMapping();
 printf("{\"schema\":\"ra2.native-viewport-probe-host.v1\",\"passed\":true,\"checks\":%u,\"assertions_active\":true,\"real_win32_mapping\":true,\"game_process_opened\":false,\"scope\":\"actual main probe and ensureBridge prior prefix; real fixture-PID mappings; only access-denied IO is injected\"}\n",checks);
}
