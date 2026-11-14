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
struct Object {uint32_t id=0,owner=1,carrier=0,type=42;int k=1,hp=100,passengers=0,mission=5,queued=-1;bool active=true,limbo=false,onMap=true,allowed=true;};
static std::unordered_map<uint32_t,Object> objects;
constexpr uint32_t transport=0x10000,passenger=0x20000;
static std::vector<uint32_t> infantry;
static uint32_t match=1;
static unsigned deployments=0,cancellations=0;
struct Shared{} shared,*sh=&shared;
static DWORD sampleClock(Shared*){return 1000;}
template<class T>static T get(uint32_t address){
 if(address==0xA8ED84)return T(10);
 auto at=objects.find(address&~0xfffu);if(at==objects.end())return T{};const auto&o=at->second;
 switch(address&0xfff){case 0:return T(o.k==1?0x7EB058:o.k==2?0x7F5C70:0x7E3EBC);case 0x10:return T(o.id);case 0x6C:return T(o.hp);case 0x74:return T(o.active);case 0x81:return T(o.limbo);case 0x90:return T(o.onMap);case 0x21C:return T(o.owner);case 0x11C:return T(o.carrier);case 0x114:return T(o.passengers);case 0xAC:return T(o.mission);case 0xB4:return T(o.queued);default:throw std::runtime_error("unexpected read");}
}
struct World {bool contains(uint32_t p)const{return objects.count(p)!=0;}} worldObjects;
struct Versions {uint32_t token=0;uint32_t get(uint32_t)const{return token;}} manualVersions;
static uint32_t typeOf(uint32_t p,int){return objects.at(p).type;}
namespace featuretypes {constexpr int UnloadPassenger=1;}
static bool featureObjectAllowed(int,uint32_t p){return objects.at(p).allowed;}
struct Vec {int count;};static Vec vec(uint32_t address){if(address!=0xA83DE8)throw std::runtime_error("unexpected vector");return {int(infantry.size())};}
static uint32_t item(Vec,int n){return infantry.at(n);}
static unloadadmission::Queue queuedUnload;
struct RecentPassenger {uint32_t p,id,carrier,carrierID,carrierType,token,tick;int frame;};
static RecentPassenger recentPassengers[nativeorders::MaxPassengers];static int recentPassengerCount;
struct UnloadPassenger {uint32_t job,p,id,carrier,carrierID,carrierType;int frame;uint32_t manualToken;};
static UnloadPassenger unloadPassengers[nativeorders::MaxPassengers];static int unloadCount;static uint32_t unloadJobSerial;
namespace taskarbiter {enum class Owner {Unload};}
static bool nativeMission(uint32_t,uint32_t,int mission,int,int,int,int,int,taskarbiter::Owner){if(mission!=16)throw std::runtime_error("unexpected mission");++deployments;return true;}
static void cancelNative(uint32_t){++cancellations;unloadCount=0;}
static bool unloadCarrierEligible(uint32_t p,uint32_t house);
#include UNLOAD_ADMISSION_HEADER
static bool unloadCarrierEligible(uint32_t p,uint32_t house){return worldObjects.contains(p)&&own(p,house)&&kind(p)==2;}
static void check(bool v,const char*why){if(!v)throw std::runtime_error(why);}
static void reset(){objects.clear();infantry={passenger};objects[transport]={10,1,0,44,2};objects[transport].passengers=1;objects[passenger]={11,1,transport};objects[passenger].limbo=true;objects[passenger].onMap=false;queuedUnload.clear();queuedUnload.begin(1,1,1000);queuedUnload.add({transport,10,0});recentPassengerCount=unloadCount=0;deployments=cancellations=0;manualVersions.token=0;}
int main(){try{
 reset();check(!own(passenger,1),"production own must reject loaded off-map passenger");buildUnloadAdmissionRoster(1);check(smartUnloadNow(1,transport)==1,"loaded carrier admission");check(unloadCount==1,"loaded passenger lost before Deploy");check(deployments==1&&unloadPassengers[0].id==11&&unloadPassengers[0].carrier==transport,"job must exist before unload");
 reset();objects[transport].passengers=0;objects[passenger].carrier=0;objects[passenger].limbo=false;objects[passenger].onMap=true;recentPassengers[0]={passenger,11,transport,10,44,0,1000,10};recentPassengerCount=1;buildUnloadAdmissionRoster(1);check(smartUnloadNow(1,transport)==1&&unloadCount==1&&deployments==0,"native-first history must still enrol");
 for(int cause=0;cause<7;++cause){reset();switch(cause){case 0:objects[passenger].owner=2;break;case 1:objects[passenger].hp=0;break;case 2:objects[passenger].active=false;break;case 3:objects[passenger].allowed=false;break;case 4:objects[passenger].carrier=0;break;case 5:objects[passenger].k=3;break;case 6:objects[passenger].carrier=0x30000;objects[0x30000]={30,1,0,44,2};break;}buildUnloadAdmissionRoster(1);smartUnloadNow(1,transport);check(unloadCount==0,"invalid passenger enrolled");}
 reset();buildUnloadAdmissionRoster(1);objects[passenger].id=99;smartUnloadNow(1,transport);check(unloadCount==0,"reused passenger identity");
 reset();buildUnloadAdmissionRoster(1);manualVersions.token=1;smartUnloadNow(1,transport);check(unloadCount==0,"new manual command overwritten");
 reset();objects[passenger].limbo=false;objects[passenger].onMap=true;infantry.push_back(passenger);buildUnloadAdmissionRoster(1);smartUnloadNow(1,transport);check(unloadCount==1,"duplicate passenger jobs");
 std::puts("PASS production unload admission: job before Deploy for loaded/limbo passenger; native-first history, owner/life/filter/carrier/kind/identity/manual-token guards and duplicate roster");return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
