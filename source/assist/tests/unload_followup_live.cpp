// Production checked Ctrl+D follow-up observer under missing native pages.
// Synthetic x86 memory only: no game process, EventClass or Host application.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unordered_map>
#include "../source/unload_followup_state.h"
static std::unordered_map<uint32_t,uint8_t> memory;
static uint32_t unreadable=0;
static unsigned reads=0,checks=0;
constexpr uint32_t A=0x210000, B=0x220000, C=0x230000, Type=0x440000, House=0x550000, Transport=0x660000;
template<class T>static void put(uint32_t at,T v){
 auto *raw=reinterpret_cast<const uint8_t*>(&v);
 for(size_t i=0;i<sizeof(T);++i)memory[at+uint32_t(i)]=raw[i];
}
static bool readBytes(uint32_t at,void*out,size_t n){
 ++reads;
 if(unreadable&&uint64_t(at)<=unreadable&&uint64_t(at)+n>unreadable){
  std::memset(out,0xC3,n);return false;
 }
 uint8_t*target=static_cast<uint8_t*>(out);
 for(size_t i=0;i<n;++i){
  auto f=memory.find(at+uint32_t(i));if(f==memory.end())return false;
  target[i]=f->second;
 }
 return true;
}
static void check(bool ok,const char*message){
 ++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::abort();}
}
static void setup(uint32_t p,uint32_t uid,bool loaded){
 // Native allocations contain private fields between the required offsets.
 for(unsigned i=0;i<0x6C8;++i)memory[p+i]=0;
 for(unsigned i=0;i<0xEC9;++i)memory[Type+i]=0;
 put<uint32_t>(p,0x7EB058);
 put<uint32_t>(p+0x10,uid);
 put<int>(p+0x6C,100);
 put<uint8_t>(p+0x74,uint8_t(!loaded));
 put<uint8_t>(p+0x81,uint8_t(loaded));
 put<uint8_t>(p+0x90,uint8_t(!loaded));
 put<int>(p+0x9C,1240+int(uid));
 put<int>(p+0xA0,3560+int(uid));
 put<int>(p+0xAC,loaded?16:2);
 put<int>(p+0xB4,loaded?16:2);
 put<uint32_t>(p+0x11C,loaded?Transport:0);
 put<uint32_t>(p+0x21C,House);
 put<uint32_t>(p+0x2B4,0);
 put<uint32_t>(p+0x5A4,0);
 put<uint32_t>(p+0x6C0,Type);
 put<uint8_t>(Type+0xEC3,1); // engineer specialist
 put<uint8_t>(Type+0xEBE,0);
 put<uint8_t>(Type+0xEC8,0);
}
static unloadfollowup::Knowledge sample(uint32_t p,uint32_t uid,unloadfollowup::Observation&out){
 return unloadfollowup::observe(p,uid,House,readBytes,out);
}
int main(){
 using unloadfollowup::Knowledge;
 setup(A,101,true);setup(B,102,true);setup(C,103,false);
 unloadfollowup::Observation a{},b{},c{};
 reads=0;
 check(sample(A,101,a)==Knowledge::Current&&a.carrier==Transport&&!a.live()&&a.engineer,
       "first passenger loaded is a valid independent task");
 check(reads<=5,"production passenger observer should use no more than 5 checked live reads");
 check(sample(B,102,b)==Knowledge::Current&&b.carrier==Transport&&b.uid!=a.uid,
       "second passenger loaded is independent");
 check(sample(C,103,c)==Knowledge::Current&&!c.carrier&&c.live()&&c.mission==2,
       "already landed passenger keeps own mission and coordinates");
 // A failure inside a batched native field span must not mutate caller output
 // or fabricate a zero-valued mission, carrier, UID, position or target.
 const auto prior=b;
 for(uint32_t field:{0u,0x10u,0x6Cu,0x74u,0x81u,0x90u,0x9Cu,0xA0u,
                     0xACu,0xB4u,0x11Cu,0x21Cu,0x2B4u,0x5A4u,0x6C0u}){
  unreadable=B+field;
  check(sample(B,102,b)==Knowledge::Unknown,"unreadable passenger native field was not unknown");
  check(!std::memcmp(&b,&prior,sizeof b),"unreadable passenger mutated live job observation");
 }
 unreadable=Type+0xEC3;
 check(sample(B,102,b)==Knowledge::Unknown&&b.carrier==prior.carrier,
       "missing specialist/deployer flags falsely advanced passenger");
 unreadable=0;
 check(sample(B,102,b)==Knowledge::Current&&b.uid==102,"restored second passenger not observable");
 // A separate, complete native change is allowed to retire the old task.
 put<uint32_t>(B+0x10,999);
 check(sample(B,102,b)==Knowledge::Invalid,"recycled passenger UID did not invalidate old followup");
 put<uint32_t>(B+0x10,102);
 put<uint32_t>(B+0x21C,House+3);
 check(sample(B,102,b)==Knowledge::Invalid,"confirmed new owner must invalidate old job");
 put<uint32_t>(B+0x21C,House);
 put<int>(B+0x6C,0);
 check(sample(B,102,b)==Knowledge::Invalid,"confirmed death must retire old job");
 put<int>(B+0x6C,100);
 // Capture/Enter to the former carrier is temporary. Enter to a new vehicle
 // must no longer be treated as the native unloading sequence.
 put<int>(C+0xAC,7);put<uint32_t>(C+0x5A4,Transport);
 check(sample(C,103,c)==Knowledge::Current&&unloadfollowup::unloadExit(c,Transport,0,100,104),
       "native Enter to the unloading carrier must remain eligible");
 put<uint32_t>(C+0x5A4,0);put<uint32_t>(C+0x2B4,0);
 check(sample(C,103,c)==Knowledge::Current&&unloadfollowup::unloadExit(c,Transport,0,100,104),
       "zero-target short native ejection grace must continue");
 check(!unloadfollowup::unloadExit(c,Transport,0,100,285),
       "stale Enter is not an unlimited native-ejection grace");
 put<uint32_t>(C+0x5A4,Transport+64);
 check(sample(C,103,c)==Knowledge::Current&&!unloadfollowup::unloadExit(c,Transport,0,100,104),
       "Enter to another vehicle must remain an explicit command");
 std::printf("PASS %u checked Ctrl+D follow-up field, three-passenger and recovery assertions (synthetic)\n",checks);
}
