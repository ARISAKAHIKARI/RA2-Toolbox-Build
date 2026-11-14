#include <cassert>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>
#include "game_entry_client.h"
#include "entry_policy.h"
using namespace gameentry;
using std::memcpy;
static unsigned checks=0,digest=2166136261u;
static std::atomic<uint64_t>managerCalls{0},validationCalls{0};
#include "manager_globals.inc"
static bool boundDead=false;
static bool isBoundThreadDead(uint32_t tid){return tid&&boundDead;}
static void refreshBoundThreadHandle(uint32_t tid){boundThreadId=tid;boundDead=false;}
namespace debugentries {enum class Reason {Unreadable};enum class Kind {Patch};enum class Hook {Pending};static void installation(Kind,unsigned,Hook,Reason) {}}
struct StubSlot {bool active=false;unsigned size=0;unsigned char original[16]{},patch[16]{};};
static struct {
 StubSlot slots[512];
 unsigned installed()const{return 0;}unsigned conflicts()const{return 0;}
 bool active(unsigned)const{return false;}bool conflict(unsigned)const{return false;}
 int find(unsigned)const{return -1;}void markConflict(unsigned){}
}patches;
static uint32_t patchModules[512];
static Result applyPatch(const Request&){return Result::PatchDenied;}
static void observePatch(unsigned){}
static bool patchRead(unsigned,void*,size_t){return false;}
static Result patchDenied(unsigned,debugentries::Reason,Result error){return error;}
#include "manager_body.inc"
extern "C" void RuntimeScopedCall(void*context,void(*body)(void*),void(*cleanup)(void*)){
 try{body(context);}catch(...){cleanup(context);throw;}cleanup(context);
}
static void check(bool ok){++checks;assert(ok);}
static void mark(unsigned value){digest=(digest^value)*16777619u;}
static Request request(Operation op,Client client=Client::Assist){Request r{};r.operation=op;r.client=client;r.module=moduleIdentity();return r;}
int main(){
 check(!active(Client::Assist));
 Request outer{};check(enter(Client::Assist,true,outer));
 check(active(Client::Assist)&&!active(Client::Cursor)&&!active(Client::Monitor));
 auto wrongModule=request(Operation::Validate);wrongModule.token=outer.token;wrongModule.module+=4096;
 check(!manage(wrongModule)&&active(Client::Assist));
 auto bad=request(Operation::Validate);bad.size=0;check(!manage(bad));bad=request(Operation::Validate);bad.version=0;check(!manage(bad));
 Request rejected{};check(!enter(Client::Cursor,false,rejected)&&active(Client::Assist));
 Request stack[127];
 for(auto&r:stack){check(capture(Client::Assist,r));check(active(Client::Assist));}
 check(!capture(Client::Assist,rejected)&&active(Client::Assist));
 auto stale=outer;stale.operation=Operation::Validate;check(!manage(stale));
 auto current=uint32_t(activeToken);activeToken=outer.token;check(!active(Client::Assist));activeToken=current;
 leave(outer);check(active(Client::Assist)); // stale outer token cannot release its child
 for(int n=126;n>=0;--n){leave(stack[n]);check(active(Client::Assist));}
 check(uint32_t(activeToken)==outer.token);
 const auto beforeContention=uint32_t(contended);
 std::atomic<bool>foreignAllowed{false};
 std::thread foreign([&]{fixtureThread=2;for(int n=0;n<1000;++n)if(active(Client::Assist))foreignAllowed=true;Request r{};if(capture(Client::Assist,r))foreignAllowed=true;});
 for(int n=0;n<1000;++n)check(active(Client::Assist));foreign.join();
 check(!foreignAllowed&&uint32_t(contended)-beforeContention==1001&&active(Client::Assist));
 mark(policy.depth);mark(policy.client);mark(policy.token);
 // Warm module lookup before observing the hot path. Each validation must
 // still consult current thread, module, client and the exact LIFO token.
 check(active(Client::Assist));fixtureLocked=0;managerCalls=validationCalls=0;
 unsigned accepted=0;for(unsigned n=0;n<100000;++n)accepted+=active(Client::Assist);
 const auto locked=fixtureLocked.load();const auto full=managerCalls.load();
 check(accepted==100000);
#ifdef HOT_VALIDATION
 check(locked==0&&full==0&&validationCalls==100000);
 check(!validateCurrent(Client::Assist,moduleIdentity()+4096,outer.token));
 check(!validateCurrent(Client::Assist,moduleIdentity(),outer.token+1));
#else
 check(locked==400000&&full==100000);
#endif
 leave(outer);check(!active(Client::Assist)&&!uint32_t(activeToken));
 boundDead=true;check(!capture(Client::Assist,rejected));check(enter(Client::Assist,true,outer));leave(outer);
 try{scoped(Client::Assist,true,[]{Request child{};check(capture(Client::Assist,child));runtimeunwind::run([]{throw 42;},[&]{leave(child);});});assert(false);}catch(int n){check(n==42);}
 check(!policy.module&&!uint32_t(activeToken)&&!busy);
 check(scoped(Client::Assist,true,[]{check(active(Client::Assist));}));
 mark(policy.depth);mark(policy.module);mark(uint32_t(activeToken));
 std::printf("{\"pass\":true,\"checks\":%u,\"semantic_digest\":%u,\"validations\":%u,\"locked_operations\":%llu,\"full_requests\":%llu}\n",checks,digest,accepted,(unsigned long long)locked,(unsigned long long)full);
}
