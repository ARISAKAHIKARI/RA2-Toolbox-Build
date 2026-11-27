#include "../source/world_capture_admission.h"
#include "../source/world_capture_requests.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
struct Leases{
 unsigned retained=0,appended=0;
 std::vector<uint32_t> last;
 void retain(uint32_t,uint32_t){++retained;}
 void append(uint32_t,const std::vector<uint32_t>&out){++appended;last=out;}
};
int main(){
 using namespace assist;
 // These UI overlay switches are passive when the separate
 // Host visual sampler owns drawing; disabling that backend stays conservative.
 constexpr uint32_t visuals[]={Capacity,Iron,Production,AllTransports,Psychic,Crates,TextPanel,Frozen};
 uint32_t all=0;
 for(auto bit:visuals){
  all|=bit;
  assert(!capturepolicy::needsNativeFlagWork(bit,true));
  assert(capturepolicy::needsNativeFlagWork(bit,false));
 }
 assert(all==capturepolicy::PassiveVisualFlags);
 assert(!capturepolicy::needsNativeFlagWork(all,true));
 assert(capturepolicy::needsNativeFlagWork(all,false));
 assert(!capturepolicy::passengerGraphNeeded(Capacity|AllTransports,true));
 assert(capturepolicy::passengerGraphNeeded(Capacity,false));
 for(uint32_t active:{AutoRepair,EjectAssist,AutoOreReturn,AdvancedInput,UnsafeTech,
                      CrateResults,AntiTheft,CustomCS,BuildAnywhere,NativeAirWaypoints}){
  assert(capturepolicy::needsNativeFlagWork(all|active,true));
 }
 for(uint32_t passenger:{EjectAssist,SmartUnloadOrders,AutoOreReturn,AdvancedEnter,AircraftSpread}){
  assert(capturepolicy::passengerGraphNeeded(passenger,true));
 }
 assert(!capturepolicy::passengerGraphNeeded(AutoRepair,true));
 assert(capturepolicy::LocalDispatchFlags==(BuildAnywhere|ReserveConstruction|DiskPrimary|SellDrained|SellVehicles));
 assert(!capturepolicy::pointerNeedsGraph(capturepolicy::LocalDispatchFlags,4,4));
 assert(!capturepolicy::pointerNeedsGraph(BuildAnywhere|ReserveConstruction,5,4));
 for(uint32_t option:{DiskPrimary,SellDrained,SellVehicles})assert(capturepolicy::pointerNeedsGraph(option,5,4));
 // A Host-only boundary consumes NO command dependency, even when hundreds
 // of thousands of subscribed display pages still live in the Host.
 std::vector<uint32_t> host,live,out;
 for(unsigned i=0;i<100000;++i)host.push_back(uint32_t(0x10000+i*4096));
 live={0x35000,0x21000,0x35000};
 Leases leases;
 capturepolicy::prepareRequestedPages(true,123,host,live,leases,out);
 assert(out.empty()&&leases.retained==0&&leases.appended==0&&live.size()==3);
 // Later user command escalates from boundary to full authoritative source.
 // All command subscriptions survive and are then included/deduplicated.
 host={0x21000,0x15000,0x21000};
 capturepolicy::prepareRequestedPages(false,140,host,live,leases,out);
 assert(leases.retained==3&&leases.appended==1&&leases.last.size()==6);
 assert((out==std::vector<uint32_t>{0x15000,0x21000,0x35000}));
 std::puts("PASS passive overlay flags use Host graph; command flags retain authority; 100k display pages bypass game-thread sort; command escalation retains requests");
}
