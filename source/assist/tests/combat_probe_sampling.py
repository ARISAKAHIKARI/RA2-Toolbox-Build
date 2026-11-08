"""Real raw-combat reads and production pair collector in a controlled memory image.

Compares batched capture with the original per-pair collector when --baseline is
provided, otherwise with the unchanged live probe path. Counts are raw field
reads/collections; they are not Windows, game-frame-time or multiplayer evidence.
"""
from pathlib import Path
import argparse
import subprocess
import tempfile


def function(text, signature):
    start = text.index(signature)
    brace = text.index("{", start)
    end, depth = brace + 1, 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--baseline", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1] / "source"
probe = (root / "combat_probe_bridge.h").read_text()
if args.baseline:
    old = args.baseline.read_text()
    legacy = function(old, "static planner::CombatProbe probeCombatPair(") + "\n" + function(old, "static void collectCombatProbes(")
    legacy = legacy.replace("probeCombatPair", "legacyProbeCombatPair").replace("collectCombatProbes", "legacyCollectCombatProbes")
else:
    legacy = function(probe, "static void collectCombatProbes(").replace("collectCombatProbes", "legacyCollectCombatProbes").replace(",&batch,n,rawOnly)", ")").replace(",&batch,n)", ")")
raw = (root / "raw_combat.h").read_text()
raw = raw.replace("static planner::Weapon rawWeaponFresh(uint32_t weapon){", "static planner::Weapon rawWeaponFresh(uint32_t weapon){++freshWeapons;")
raw = raw.replace("planner::Actor& a,bool weapons,bool cached=false){", "planner::Actor& a,bool weapons,bool cached=false){if(weapons)++sourceSamples;else ++targetSamples;")
code = r'''
#include "protocol.h"
#include "combat_query_policy.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include <unordered_map>
using namespace assist;
static std::vector<unsigned char> memory(0xB00000);
static uint32_t nextAddress=0x20000,match=1;
static uint64_t reads,sourceSamples,targetSamples,freshWeapons,nativeCalls;
static struct{uint32_t nativeCombatQueries=0,advancedRangeCells=4;}shared;
static auto*sh=&shared;
template<class T>static T get(uint32_t at){++reads;assert(at+sizeof(T)<=memory.size());T v;std::memcpy(&v,memory.data()+at,sizeof v);return v;}
template<class T>static void put(uint32_t at,T v){assert(at+sizeof(T)<=memory.size());std::memcpy(memory.data()+at,&v,sizeof v);}
static uint32_t alloc(){auto p=nextAddress;nextAddress+=0x2000;assert(nextAddress<0xA00000);return p;}
static int kind(uint32_t p){uint32_t v=get<uint32_t>(p);return v==0x7F5C70?2:v==0x7EB058?1:v==0x7E3EBC?3:v==0x7E22A4?0:-1;}
static uint32_t typeOf(uint32_t p,int k){return k>=0?get<uint32_t>(p+(k==3?0x520:k==1?0x6C0:0x6C4)):0;}
static uint32_t rawObjectID(uint32_t p){return p>=0x10000?get<uint32_t>(p+0x10):0;}
static bool featureTypeAllowed(featuretypes::Feature,uint32_t){return true;}
static std::unordered_map<uint32_t,uint32_t> objects;
static std::vector<uint32_t> sources,targets;
static uint32_t toggleNativeTarget,mutationType,mutationTarget,mutationWeapon;static int nativeStatus=1;
static uint32_t worldByID(uint32_t id){
 auto it=objects.find(id);if(it==objects.end()||get<uint32_t>(it->second+0x10)!=id)return 0;
 if(id==toggleNativeTarget)sh->nativeCombatQueries=1;
 return it->second;
}
static struct{int mode;}nativeOrders[128];
static int orderIndex(uint32_t p){for(unsigned n=0;n<sources.size();++n)if(sources[n]==p)return int(n);return -1;}
static bool burstOwns(uint32_t){return false;}
static bool dodgeOwns(uint32_t){return false;}
static bool own(uint32_t,uint32_t){return true;}
static bool nativeEnemy(uint32_t,uint32_t){return true;}
struct Vec{int count;};static Vec vec(uint32_t){return {int(targets.size())};}
static uint32_t item(Vec,int n){return targets[n];}
@RAW@
extern "C" int GuardedObjectAir(unsigned p,int*out){*out=rawObjectAirborne(p,kind(p));return 1;}
extern "C" int GuardedCombatWeapon(unsigned p,unsigned q,unsigned*w,int*slot,int*close,int*air){
 ++nativeCalls;
 if(mutationType){put<uint32_t>(mutationType+0x898,mutationWeapon);sh->nativeCombatQueries=0;}
 if(mutationTarget){put<int>(mutationTarget+0xA4,700);put<bool>(mutationTarget+0x134,true);sh->nativeCombatQueries=0;}
 *w=rawWeapon(typeOf(p,kind(p)),get<float>(p+0x150),0);*slot=0;*close=1;*air=rawObjectAirborne(q,kind(q));return nativeStatus;
}
#include "combat_query_bridge.h"
#include "combat_probe_bridge.h"
@LEGACY@
static void check(bool value,const char*why){if(!value){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
static uint32_t weapon(int range=4096,bool air=false,bool ground=true){
 auto w=alloc(),projectile=alloc(),warhead=alloc();
 put<int>(w+0xB4,range);put<int>(w+0xB8,64);put<uint32_t>(w+0xA0,projectile);put<uint32_t>(w+0xAC,warhead);
 put<bool>(projectile+0x2A4,air);put<bool>(projectile+0x2A5,ground);
 for(int n=0;n<11;++n)put<double>(warhead+0xA0+n*8,1.);
 return w;
}
static uint32_t object(uint32_t type,int k,uint32_t id,int x,int y){
 auto p=alloc();const uint32_t vt[]={0x7E22A4,0x7EB058,0x7F5C70,0x7E3EBC};put<uint32_t>(p,vt[k]);
 put<uint32_t>(p+0x10,id);put<uint32_t>(p+(k==3?0x520:k==1?0x6C0:0x6C4),type);
 put<int>(p+0x9C,x);put<int>(p+0xA0,y);put<int>(p+0xA4,0);put<int>(p+0x2FC,10);
 objects[id]=p;return p;
}
static planner::Actor actor(uint32_t p,bool source){
 planner::Actor a{};a.id=get<uint32_t>(p+0x10);a.type=typeOf(p,kind(p));a.x=get<int>(p+0x9C);a.y=get<int>(p+0xA0);
 a.flags=source?planner::Own:planner::Enemy;rawCombatInfo(p,a.type,kind(p),a,source,true);return a;
}
static void build(planner::World&w,int ns,int nt,int variant){
 memory.assign(memory.size(),0);nextAddress=0x20000;objects.clear();sources.clear();targets.clear();sh->nativeCombatQueries=0;
 toggleNativeTarget=mutationType=mutationTarget=mutationWeapon=0;nativeStatus=1;std::memset(weaponCache,0,sizeof weaponCache);w={};w.match=++match;w.frame=100;put<int>(0xA8ED84,100);
 auto primary=weapon(),secondary=weapon(5000,true,true),elite=weapon(3000,true,true);
 auto primaryWH=get<uint32_t>(primary+0xAC),secondaryWH=get<uint32_t>(secondary+0xAC);
 if(variant==1){put<double>(primaryWH+0xA0+4*8,0.);put<double>(secondaryWH+0xA0+5*8,0.);}
 if(variant==2){put<bool>(primary+0x15C,true);put<bool>(primaryWH+0x15B,true);}
 if(variant==3)put<bool>(primaryWH+0x155,true);
 if(variant==4)put<bool>(primaryWH+0x159,true);
 if(variant==5)put<bool>(primaryWH+0x15A,true);
 if(variant==6)put<bool>(primaryWH+0x178,true);
 for(int n=0;n<ns;++n){
  auto type=alloc();put<bool>(type+0x231,true);put<bool>(type+0xD3A,true);put<bool>(type+0xD38,true);
  put<int>(type+0x68C,256);put<uint32_t>(type+0x898,primary);put<uint32_t>(type+0x898+28,secondary);
  put<uint32_t>(type+0xA94,elite);put<uint32_t>(type+0xA94+28,secondary);
  const int k=(variant==7)?n%4:2;auto p=object(type,k,100+n,2048+n*32,2048);
  if(variant==8){put<bool>(type+0xCD5,true);put<int>(p+0x140,2);put<uint32_t>(type+0x898+4*28,primary);put<uint32_t>(type+0x898+5*28,secondary);}
  if(variant==9){put<int>(type+0x808,1);put<int>(p+0x138,1);}
  if(variant==10)put<float>(p+0x150,2.0f);
  if(variant==11){put<int>(type+0x684,2);put<int>(p+0x2FC,0);}
  if(variant==12){put<bool>(type+0x6AC,true);put<int>(type+0x6A8,1);put<bool>(p+0x6E0,true);}
  sources.push_back(p);w.actors[w.count++]=actor(p,true);
 }
 for(int n=0;n<nt;++n){
  auto type=alloc();put<bool>(type+0x231,true);put<bool>(type+0xD3A,n%3!=0);put<bool>(type+0xD38,n%3!=1);put<int>(type+0x9C,n%11);
  put<bool>(type+0xD35,n%4==0);put<bool>(type+0xD36,n%4==1);put<bool>(type+0xD97,n%5==0);
  auto p=object(type,n%4,1000+n,2060+(n%8)*700,2048+(n%3)*100);put<int>(p+0xA4,(n%3)*128);
  if(n%4==2)put<bool>(p+0x134,true);
  targets.push_back(p);w.actors[w.count++]=actor(p,false);
 }
 // A fresh selector and the published/cache-backed compatibility view are
 // intentionally different. Reusing World.weapons would choose the wrong slot.
 if(variant==13)for(int n=0;n<11;++n)put<double>(primaryWH+0xA0+n*8,0.);
 if(variant==14)put<bool>(sources[0]+0x1C8,true);
 if(variant==15)put<uint32_t>(targets[0]+0x10,0xdeadbeef);
 if(variant==16){put<int>(primary+0xB4,32);put<int>(primary+0xB8,64);}
}
struct Counts{uint64_t read,source,target,fresh,native;};
static Counts counts(){return {reads,sourceSamples,targetSamples,freshWeapons,nativeCalls};}
static void zeroCounts(){reads=sourceSamples=targetSamples=freshWeapons=nativeCalls=0;}
static void equal(const planner::World&a,const planner::World&b){
 check(a.combatCount==b.combatCount,"probe count differs");
 check(std::memcmp(a.combat,b.combat,size_t(a.combatCount)*sizeof(a.combat[0]))==0,"probe flags, selected slot or effectiveness differs");
 for(int n=0;n<a.count;++n)check(std::memcmp(&a.actors[n],&b.actors[n],sizeof(a.actors[n]))==0,"published Actor/probe coverage differs");
}
int main(){
 auto input=std::make_unique<planner::World>(),legacy=std::make_unique<planner::World>(),batched=std::make_unique<planner::World>();
 static WeaponCacheEntry cacheBefore[1024];
 for(int variant=0;variant<=16;++variant){
  build(*input,6,12,variant);*legacy=*input;*batched=*input;std::memcpy(cacheBefore,weaponCache,sizeof weaponCache);
  zeroCounts();legacyCollectCombatProbes(*legacy);auto before=counts();std::memcpy(weaponCache,cacheBefore,sizeof weaponCache);
  zeroCounts();collectCombatProbes(*batched);auto after=counts();equal(*legacy,*batched);
  check(after.source<=6&&after.target<=12,"raw Actor capture repeated within one batch");
  if(variant==0)check(before.source==72&&before.target==72&&after.source==6&&after.target==12,"source/target sample counts");
  // A second publication must reacquire changed fields, including during the
  // same logic frame. No object sample is retained across collector calls.
  put<int>(targets[0]+0xA4,8000);*legacy=*input;*batched=*input;
  std::memcpy(weaponCache,cacheBefore,sizeof weaponCache);legacyCollectCombatProbes(*legacy);
  std::memcpy(weaponCache,cacheBefore,sizeof weaponCache);collectCombatProbes(*batched);equal(*legacy,*batched);
 }
 std::puts("PASS 17 raw selector branches, stale metadata cache and recapture on every publication");
 build(*input,128,256,0);*legacy=*input;*batched=*input;std::memcpy(cacheBefore,weaponCache,sizeof weaponCache);
 zeroCounts();legacyCollectCombatProbes(*legacy);auto before=counts();std::memcpy(weaponCache,cacheBefore,sizeof weaponCache);
 zeroCounts();collectCombatProbes(*batched);auto after=counts();equal(*legacy,*batched);
 check(before.source==32768&&before.target==32768&&after.source==128&&after.target==256,"full pair-budget sample count");
 std::printf("PASS 128 sources x 256 targets: Actor captures %llu+%llu -> %llu+%llu; fresh Weapon reads %llu -> %llu; field reads %llu -> %llu\n",
  (unsigned long long)before.source,(unsigned long long)before.target,(unsigned long long)after.source,(unsigned long long)after.target,
  (unsigned long long)before.fresh,(unsigned long long)after.fresh,(unsigned long long)before.read,(unsigned long long)after.read);
 for(int status: {-1,0,1}){
  build(*input,3,4,0);sh->nativeCombatQueries=1;nativeStatus=status;*legacy=*input;*batched=*input;
  zeroCounts();legacyCollectCombatProbes(*legacy);auto old=counts();zeroCounts();collectCombatProbes(*batched);auto now=counts();equal(*legacy,*batched);
  check(old.native==now.native&&now.source==0&&now.target==0,"native queries must remain immediate per pair, including fault/ineligible branches");
 }
 std::puts("PASS native experimental calls, zero/fault status and source-atomic coverage");
 build(*input,3,4,0);sh->nativeCombatQueries=1;*batched=*input;zeroCounts();collectCombatProbes(*batched,true);
 check(nativeCalls==0&&sourceSamples==3&&targetSamples==4,"explicit pure capture must never enter a native selector");
 std::puts("PASS explicit raw collector stays pure while native configuration is enabled");
 for(int targetChange=0;targetChange<2;++targetChange){
  build(*input,3,4,0);toggleNativeTarget=get<uint32_t>(targets[targetChange?3:1]+0x10);
  if(targetChange)mutationTarget=targets[0];else{mutationType=typeOf(sources[0],kind(sources[0]));mutationWeapon=weapon(1800,true,true);}
  auto memoryBefore=memory;*legacy=*input;*batched=*input;std::memcpy(cacheBefore,weaponCache,sizeof weaponCache);
  legacyCollectCombatProbes(*legacy);memory=memoryBefore;sh->nativeCombatQueries=0;std::memcpy(weaponCache,cacheBefore,sizeof weaponCache);
  collectCombatProbes(*batched);equal(*legacy,*batched);
 }
 std::puts("PASS raw/native/raw transition discards source and target samples after native call");
 build(*input,1,2,0);zeroCounts();for(int n=0;n<2;++n)probeCombatPair(sources[0],targets[n],input->actors[0],input->actors[n+1],0);
 check(sourceSamples==2&&targetSamples==2,"standalone live probe must keep immediate collection");
 std::puts("PASS standalone live admission probe remains uncached");
}
'''.replace("@RAW@", raw).replace("@LEGACY@", legacy)
with tempfile.TemporaryDirectory(prefix="combat-probe-sampling-") as tmp:
    cpp, exe = Path(tmp) / "test.cpp", Path(tmp) / "test"
    cpp.write_text(code)
    subprocess.run(["g++", "-std=c++17", "-O2", "-I", str(root), str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
