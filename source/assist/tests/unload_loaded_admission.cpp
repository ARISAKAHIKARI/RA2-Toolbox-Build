// Production roster/admission and production live/own predicates, synthetic
// object memory and native Deploy delivery. No game executable is used.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "../source/native_orders.h"
#include "../source/unload_policy.h"
#include "../source/unload_admission.h"
using namespace assist;
using DWORD=uint32_t;
struct Object {uint32_t id=0,owner=1,carrier=0,type=42;int k=1,hp=100,passengers=0,mission=5,queued=-1;bool active=true,limbo=false,onMap=true,allowed=true,deployed=false;};
static std::unordered_map<uint32_t,Object> objects;
constexpr uint32_t transport=0x10000,passenger=0x20000;
static std::vector<uint32_t> infantry;
static uint32_t match=1;static int indexedFrame=10;static struct {bool chat=false;} snap;
static unsigned deployments=0,cancellations=0;
struct Shared{uint32_t flags=1;} shared,*sh=&shared;
constexpr uint32_t SmartUnloadOrders=1;
static bool loadInterfaces(){return true;}
static bool simulationReady(){return true;}
static DWORD sampleClock(Shared*){return 1000;}
template<class T>static T get(uint32_t address){
 if(address==0xA8ED84)return T(10);
 auto at=objects.find(address&~0xfffu);if(at==objects.end())return T{};const auto&o=at->second;
 switch(address&0xfff){case 0:return T(o.k==1?0x7EB058:o.k==2?0x7F5C70:0x7E3EBC);case 0x10:return T(o.id);case 0x6C:return T(o.hp);case 0x74:return T(o.active);case 0x81:return T(o.limbo);case 0x90:return T(o.onMap);case 0x21C:return T(o.owner);case 0x11C:return T(o.carrier);case 0x114:return T(o.passengers);case 0x118:return T(o.type==44&&o.k==2?0x20000:0);case 0x30:return T(o.carrier?o.type==42?0:0:0);case 0x1C8:return T(o.deployed);case 0xAC:return T(o.mission);case 0xB4:return T(o.queued);default:throw std::runtime_error("unexpected read");}
}
struct World {bool contains(uint32_t p)const{return objects.count(p)!=0;}} worldObjects;
struct Versions {uint32_t token=0,carrierMove=0;uint32_t get(uint32_t id)const{return token+(id==10?carrierMove:0);}} manualVersions;
static uint32_t typeOf(uint32_t p,int){return objects.at(p).type;}
namespace featuretypes {constexpr int UnloadPassenger=1,UnloadTransport=2;}
static bool featureObjectAllowed(int,uint32_t p){return objects.at(p).allowed;}
static std::vector<uint32_t> selectedCarriers{transport};
struct Vec {int count;uint32_t address;};static Vec vec(uint32_t address){if(address!=0xA83DE8&&address!=0xA8ECB8)throw std::runtime_error("unexpected vector");return {int(address==0xA83DE8?infantry.size():selectedCarriers.size()),address};}
static uint32_t item(Vec v,int n){return (v.address==0xA83DE8?infantry:selectedCarriers).at(n);}
static unloadadmission::Queue queuedUnload;
struct RecentPassenger {uint32_t p,id,carrier,carrierID,carrierType,token,tick;int frame;};
static RecentPassenger recentPassengers[nativeorders::MaxPassengers];static int recentPassengerCount;
struct UnloadPassenger {uint32_t job,p,id,carrier,carrierID,carrierType;int frame;uint32_t manualToken;};
static UnloadPassenger unloadPassengers[nativeorders::MaxPassengers];static int unloadCount;static uint32_t unloadJobSerial;
namespace taskarbiter {enum class Owner {Unload};}
static bool nativeMission(uint32_t,uint32_t,int mission,int,int,int,int,int,taskarbiter::Owner){if(mission!=16)throw std::runtime_error("unexpected mission");++deployments;return true;}
static void finishUnloadFollowup(const UnloadPassenger&,int){}
#ifdef USE_PRODUCTION_CARRIER_AND_COMMAND
static void releaseSticky(uint32_t){}
static void pauseGlobalMagnetron(uint32_t){}
static void cancelBurst(uint32_t){}
static void cancelDodge(uint32_t){}
static void cancelOreJob(uint32_t p){if(p==transport)++manualVersions.carrierMove;}
static void cancelLoadingJobs(uint32_t){}
static int orderIndex(uint32_t){return -1;}
static void removeOrder(int){}
#else
static void cancelNative(uint32_t){++cancellations;unloadCount=0;}
#endif
static bool unloadCarrierEligible(uint32_t p,uint32_t house);
#include UNLOAD_ADMISSION_HEADER
#ifndef USE_PRODUCTION_CARRIER_AND_COMMAND
static bool unloadCarrierEligible(uint32_t p,uint32_t house){return worldObjects.contains(p)&&own(p,house)&&kind(p)==2;}
#endif
static void check(bool v,const char*why){if(!v)throw std::runtime_error(why);}
static void reset(){objects.clear();infantry={passenger};selectedCarriers={transport};objects[transport]={10,1,0,44,2};objects[transport].passengers=1;objects[passenger]={11,1,transport};objects[passenger].limbo=true;objects[passenger].onMap=false;queuedUnload.clear();queuedUnload.begin(1,1,1000);queuedUnload.add({transport,10,0});recentPassengerCount=unloadCount=0;deployments=cancellations=0;manualVersions.token=manualVersions.carrierMove=0;}
int main(){try{
 reset();check(!own(passenger,1),"production own must reject loaded off-map passenger");buildUnloadAdmissionRoster(1);check(smartUnloadNow(1,transport)==1,"loaded carrier admission");check(unloadCount==1,"loaded passenger lost before Deploy");check(deployments==1&&unloadPassengers[0].id==11&&unloadPassengers[0].carrier==transport,"job must exist before unload");
 reset();objects[transport].passengers=0;objects[passenger].carrier=0;objects[passenger].limbo=false;objects[passenger].onMap=true;recentPassengers[0]={passenger,11,transport,10,44,0,1000,10};recentPassengerCount=1;buildUnloadAdmissionRoster(1);check(smartUnloadNow(1,transport)==1&&unloadCount==1&&deployments==0,"native-first history must still enrol");
 for(int cause=0;cause<7;++cause){reset();switch(cause){case 0:objects[passenger].owner=2;break;case 1:objects[passenger].hp=0;break;case 2:objects[passenger].active=false;break;case 3:objects[passenger].allowed=false;break;case 4:objects[passenger].carrier=0;break;case 5:objects[passenger].k=3;break;case 6:objects[passenger].carrier=0x30000;objects[0x30000]={30,1,0,44,2};break;}buildUnloadAdmissionRoster(1);smartUnloadNow(1,transport);check(unloadCount==0,"invalid passenger enrolled");}
 reset();buildUnloadAdmissionRoster(1);objects[passenger].id=99;smartUnloadNow(1,transport);check(unloadCount==0,"reused passenger identity");
 reset();buildUnloadAdmissionRoster(1);manualVersions.token=1;smartUnloadNow(1,transport);check(unloadCount==0,"new manual command overwritten");
#ifdef USE_PRODUCTION_CARRIER_AND_COMMAND
 // A manual Move on the carrier cannot cancel the disembarked passenger's
 // independently-owned follow-up; a manual order on that passenger can.
 reset();buildUnloadAdmissionRoster(1);
 check(smartUnloadNow(1,transport)==1&&unloadCount==1,"enrol passenger before movement");
 objects[transport].passengers=0;objects[passenger].carrier=0;
 objects[passenger].limbo=false;objects[passenger].onMap=true;
 cancelNative(transport);
 check(unloadCount==1&&unloadPassengers[0].id==11,
       "carrier Move must preserve independent disembarked passenger followup");
 cancelNative(passenger);
 check(unloadCount==0,"manual passenger Move must cancel its own followup");
 // If the native unload finishes before command admission, moving the
 // empty carrier must also retain that pending latched Ctrl+D.
 reset();observeUnloadRoster(1,10,1000);
 objects[transport].passengers=0;objects[passenger].carrier=0;
 objects[passenger].limbo=false;objects[passenger].onMap=true;
 cancelNative(transport);
 check(queuedUnload.count==1,"empty carrier Move must retain pending Ctrl+D roster");
 check(manualVersions.get(10)!=queuedUnload.carriers[0].token,
       "carrier Move must advance the manual version in the fixture");
 check(unloadadmission::retainAfterCarrierMove(queuedUnload.carriers[0].token,manualVersions.get(10),0),
       "completed unload may enrol passengers across transport manual version change");
 check(!unloadadmission::retainAfterCarrierMove(queuedUnload.carriers[0].token,manualVersions.get(10),1),
       "loaded carrier manual Move must not resurrect stale native unloading");
 buildUnloadAdmissionRoster(1);
 check(smartUnloadNow(1,transport)==1&&unloadCount==1,
       "late admission after carrier Move must still enrol disembarked passenger");
 reset();cancelNative(transport);
 check(queuedUnload.count==0,"manual Move with passengers still aboard cancels unstarted unload");
 reset();objects[transport].deployed=true;objects[transport].mission=16;
 buildUnloadAdmissionRoster(1);check(smartUnloadNow(1,transport)==1&&unloadCount==1&&deployments==0,
  "native-first Deploy must not discard a Ctrl+D passenger or send duplicate Deploy");
 reset();infantry.clear();buildUnloadAdmissionRoster(1);
 check(smartUnloadNow(1,transport)==2&&unloadCount==0&&deployments==1&&cancellations==0,
  "vehicle-only or temporarily absent infantry must issue one Deploy and retain the pending command");
 check(smartUnloadNow(1,transport,false)<0&&deployments==1,
  "pending missing-passenger retry must not issue a duplicate native Deploy");
 reset();observeUnloadRoster(1,10,1000);
 check(recentPassengerCount==1,"ordinary loaded passenger must be remembered before the first Ctrl+D");
 objects[transport].passengers=0;objects[transport].deployed=true;objects[transport].mission=16;
 objects[passenger].carrier=0;objects[passenger].limbo=false;objects[passenger].onMap=true;
 check(smartUnload(1)==1&&queuedUnload.count==1,"native D preceding hotkey must still select the carrier from history");
 buildUnloadAdmissionRoster(1);
 check(smartUnloadNow(1,transport)==1&&unloadCount==1&&deployments==0,
  "single Ctrl+D must continue after D already ejected the passenger");
 reset();check(smartUnload(1)==1&&recentPassengerCount==1&&queuedUnload.count==1,
  "first Ctrl+D must latch selected passenger identity before game unloads");
 objects[transport].deployed=true;objects[transport].passengers=0;objects[transport].mission=16;
 objects[passenger].carrier=0;objects[passenger].limbo=false;objects[passenger].onMap=true;
 buildUnloadAdmissionRoster(1);check(smartUnloadNow(1,transport)==1&&unloadCount==1&&deployments==0,
  "first Ctrl+D command snapshot must survive native-first ejection without second key press");
#endif
 // Real carrier cargo is a linked list; later occupants can remain
 // inactive/limbo until their own turn to exit.
 reset();
 const uint32_t second=0x30000,third=0x40000;
 objects[transport].passengers=3;
 objects[passenger].type=42;
 objects[second]={12,1,transport,43,1};objects[third]={13,1,transport,45,1};
 objects[second].active=false;objects[third].active=false;
 objects[second].limbo=objects[third].limbo=true;
 objects[second].onMap=objects[third].onMap=false;
 infantry.push_back(second);infantry.push_back(third);
 buildUnloadAdmissionRoster(1);
 check(smartUnloadNow(1,transport)==1&&unloadCount==3,
       "all three loaded passengers must enroll, not just the active first exit");
 bool gotSecond=false,gotThird=false;
 for(int i=0;i<unloadCount;++i){gotSecond|=unloadPassengers[i].id==12;gotThird|=unloadPassengers[i].id==13;}
 check(gotSecond&&gotThird&&deployments==1,"each passenger owns one followup, single Deploy");
 // When the first one has already landed, the two remaining passengers
 // must be carried forward with the history snapshot and native chain.
 reset();
 objects[transport].passengers=2;objects[second]={12,1,transport,43,1};objects[third]={13,1,transport,45,1};
 objects[second].active=objects[third].active=false;
 infantry.push_back(second);infantry.push_back(third);
 objects[passenger].carrier=0;objects[passenger].onMap=true;
 recentPassengers[0]={passenger,11,transport,10,44,0,1000,10};recentPassengerCount=1;
 buildUnloadAdmissionRoster(1);
 check(smartUnloadNow(1,transport)==1&&unloadCount==3,
       "native-first staggered ejection must retain all three passengers");
 reset();objects[passenger].limbo=false;objects[passenger].onMap=true;infantry.push_back(passenger);buildUnloadAdmissionRoster(1);smartUnloadNow(1,transport);check(unloadCount==1,"duplicate passenger jobs");
 std::puts("PASS production unload admission: job before Deploy for loaded/limbo passenger; native-first history, owner/life/filter/carrier/kind/identity/manual-token guards and duplicate roster");return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
