#pragma once
#include <stdint.h>
// One ABI, one exported function, one instance in the target process.
namespace gameentry {
// v4 isolates the strict no-code-write manager from older pinned Runtimes.
constexpr uint32_t Version=4;
inline constexpr bool CodeWritesAllowed=false;
inline bool strictCodeWritePolicy(const uint32_t* info){return info&&info[0]==1&&info[1]==0&&info[2]==0;}
enum class Operation:uint32_t {Enter,Leave,Patch,Inspect,MarkConflict,Capture,Validate};
enum class Client:uint32_t {Assist=1,Cursor=2,Monitor=3};
enum class Result:int32_t {Ok=1,Busy=-1,WrongThread=-2,WrongOwner=-3,Invalid=-4,PatchDenied=-5,SystemFailure=-6};
struct Request {
 uint32_t size=sizeof(Request),version=Version;
 Operation operation=Operation::Inspect;
 Client client=Client::Assist;
 uint32_t module=0,token=0,bindThread=0,address=0,length=0;
 uint8_t before[16]{},after[16]{};
 uint32_t gameThread=0,activeModule=0,installed=0,conflicts=0,active=0,conflict=0;
 uint32_t entries=0,reentrant=0,contended=0,wrongThread=0;
};
static_assert(sizeof(Request)==108,"entry manager ABI");
}
