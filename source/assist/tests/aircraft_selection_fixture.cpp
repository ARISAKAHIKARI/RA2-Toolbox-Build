// Small compiled fixture for the production helper, using actual native code
// from the owner's separately supplied supported executable during execution.
#include <stdint.h>
#include <stddef.h>
static uint32_t deniedAddress=0;
static bool entryActive=true;
static bool read(uint32_t address,void*out,size_t size){
 if(address<0x10000||uint64_t(address)+size>=0x80000000ull||
    (deniedAddress&&address<=deniedAddress&&uint64_t(address)+size>deniedAddress))return false;
 auto*source=reinterpret_cast<const uint8_t*>(uintptr_t(address));auto*target=static_cast<uint8_t*>(out);
 for(size_t i=0;i<size;++i)target[i]=source[i];return true;
}
template<class T>static T get(uint32_t address){T value{};read(address,&value,sizeof value);return value;}
static int kind(uint32_t p){auto v=get<uint32_t>(p);return v==0x7E22A4?0:v==0x7F5C70?2:v==0x7EB058?1:v==0x7E3EBC?3:-1;}
static bool live(uint32_t p){return kind(p)>=0&&get<int>(p+0x6C)>0&&get<uint8_t>(p+0x74)&&!get<uint8_t>(p+0x81)&&get<uint8_t>(p+0x90);}
static uint32_t lastHouse=0x20400000;
namespace gameentry {enum class Client {Assist};static bool active(Client){return entryActive;}}
template<class T>static T call(uint32_t address,uint32_t self){return reinterpret_cast<T(__attribute__((thiscall))*)(uint32_t)>(uintptr_t(address))(self);}
#include "../source/aircraft_selection_bridge.h"
extern "C" __declspec(dllexport) int selectionFixture(uint32_t p,int operation,uint32_t deny){
 deniedAddress=deny;entryActive=operation!=2;
 return operation==0||operation==2?dispatchedAircraftSelectionReady(p):deselectDispatchedAircraft(p);
}
