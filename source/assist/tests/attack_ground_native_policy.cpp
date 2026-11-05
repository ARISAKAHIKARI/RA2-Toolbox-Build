// Production SP adapter with synthetic memory/native calls. Not game execution.
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <unordered_map>
#include <vector>
using std::size_t;
static std::unordered_map<uint32_t,unsigned char> memory;
static const uint32_t actor=0x2001000;
static bool active=true,single=true,ready=true,alive=true,suicide=false,stamp=true;
static int actorKind=1,error=0,voices=0,cancels=0,fires=0,checks=0;
static uint32_t bullet=0x2030000;
static std::vector<int> sequence;
template<class T>static T get(uint32_t p){T out{};auto*bytes=(unsigned char*)&out;for(size_t n=0;n<sizeof out;++n)bytes[n]=memory[p+uint32_t(n)];return out;}
template<class T>static void put(uint32_t p,T v){auto*bytes=(unsigned char*)&v;for(size_t n=0;n<sizeof v;++n)memory[p+uint32_t(n)]=bytes[n];}
static bool read(uint32_t p,void*out,size_t n){for(size_t i=0;i<n;++i){if(!memory.count(p+uint32_t(i)))return false;((unsigned char*)out)[i]=memory[p+uint32_t(i)];}return true;}
static int kind(uint32_t p){assert(p==actor);return actorKind;}
static bool simulationReady(){return ready;}
namespace eventdelivery{static bool isSingleplayer(int){return single;}}
namespace gameentry{enum class Client{Assist};static bool active(Client){return ::active;}}
static void cancelNative(uint32_t p,bool){assert(p==actor&&alive);++cancels;sequence.push_back(2);}
static void inputFeedback(uint32_t p,int,bool){assert(p==actor&&alive);++voices;sequence.push_back(3);}
template<class R,class... A>static R call(uint32_t address,uint32_t self,A...args){
 uint32_t values[]={uint32_t(args)...};assert(self==actor&&values[0]==actor&&values[1]==0&&alive);
 if constexpr(sizeof...(A)==3){assert(address==uint32_t(actorKind==1?0x51c8b0:0x740fd0)&&values[2]==1);++checks;sequence.push_back(1);return R(error);}
 else{assert(address==uint32_t(actorKind==1?0x51df60:0x741340));++fires;sequence.push_back(4);if(stamp){put(actor+0x120,100);put(actor+0x2ec,100);put(actor+0x2f0,0);put(actor+0x2f4,30);}if(suicide)alive=false;return R(bullet);}
}
#include "../source/attack_ground_native_bridge.h"
static void bytes(uint32_t at,const std::vector<unsigned char>&value){for(auto b:value)memory[at++]=b;}
static void fresh(int k=1){
 memory.clear();active=single=ready=alive=stamp=true;suicide=false;actorKind=k;error=voices=cancels=fires=checks=0;bullet=0x2030000;sequence.clear();
 const uint32_t vt=k==1?0x7eb058:0x7f5c70;put(actor,vt);put(vt+0x3c0,k==1?0x51c8b0u:0x740fd0u);put(vt+0x3cc,k==1?0x51df60u:0x741340u);put(0xa8ed84,100);
 bytes(0x51c8b0,{0x83,0xec,0x14,0x53,0x8b,0xd9});bytes(0x740fd0,{0x83,0xec,0x0c,0x53,0x55,0x56});
 bytes(0x51df60,{0x8b,0x44,0x24,0x08,0x56,0x8b,0xf1});bytes(0x741340,{0x83,0xec,0x10,0x56,0x8b,0xf1});bytes(0x6fdd50,{0x55,0x8b,0xec,0x83,0xe4,0xf8});
}
int main(){
 unsigned total=0;auto check=[&](bool value){++total;assert(value);};bool voiced=false;
 for(int k:{1,2}){fresh(k);voiced=false;check(ownGroundShot(actor,0,voiced)==1);check(checks==1&&fires==1&&voices==1&&cancels==1&&voiced);check(sequence==std::vector<int>({1,2,3,4}));}
 for(int e:{1,2,3,5,6,7,8}){fresh();error=e;voiced=false;check(ownGroundShot(actor,0,voiced)==0);check(!fires&&!voices&&!cancels&&!voiced);}
 fresh();single=false;voiced=false;check(ownGroundShot(actor,0,voiced)==-43&&checks==0);
 fresh();active=false;check(ownGroundShot(actor,0,voiced)==-43&&checks==0);
 fresh();ready=false;check(ownGroundShot(actor,0,voiced)==-43&&checks==0);
 fresh();put(actor,0x7eb059u);check(ownGroundShot(actor,0,voiced)==-43&&checks==0);
 fresh();memory[0x51df60]=0xe9;check(ownGroundShot(actor,0,voiced)==-43&&checks==0);
 fresh();memory.erase(0x6fdd50);check(ownGroundShot(actor,0,voiced)==-43&&checks==0);
 fresh();check(ownGroundShot(actor,18,voiced)==-43&&checks==0);
 fresh();suicide=true;voiced=false;check(ownGroundShot(actor,0,voiced)==1&&!alive&&voices==1&&sequence.back()==4);
 fresh();voiced=true;check(ownGroundShot(actor,0,voiced)==1&&voices==0);
 fresh();bullet=0;voiced=false;check(ownGroundShot(actor,0,voiced)==1&&voices==1);
 fresh();bullet=0;stamp=false;voiced=false;check(ownGroundShot(actor,0,voiced)==0&&voices==1);
 std::printf("PASS %u production native shot adapter checks\n",total);
}
