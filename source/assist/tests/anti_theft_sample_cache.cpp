// Included after the existing tactical fixture's controlled memory/Win32 seams
// and the actual production AntiTheft/planner collector functions.
#include <chrono>
static unsigned checks=0,scenarios=0;
static uint32_t captureSerial=0;
static void check(bool value,const char* reason){++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",reason);std::exit(1);}}
static void vectorAt(uint32_t at,uint32_t items,const std::vector<uint32_t>& list){
 Vec v{0,items,int(list.size()),0,int(list.size()),0};std::memcpy(memory.data()+at,&v,sizeof v);
 if(!list.empty())std::memcpy(memory.data()+items,list.data(),list.size()*4);
}
static void seal(uint32_t omit=0,bool failed=false){
 check(!sampleReadDepth,"scope leaked into capture");worldFrameReady=false;
 worldframe::Key key{123,456,match,++captureSerial,clockNow,worldframe::Base,0x60000,0x400000,lastHouse,512*512,snap.frame,3,7,8,9,10};
 worldFrame.reset(key);runtimeWorldDemands.clear();runtimeDemandBits.fill(0);missingPage=omit&~4095u;
 for(uint32_t p=0x10000;p<memory.size();p+=4096){if(p==missingPage&&!failed)continue;
  auto acquire=[&](uint32_t at,void* out,size_t n){if(missingPage&&at<missingPage+4096&&uint64_t(at)+n>missingPage)return false;return liveRead(at,out,n);};
  worldFrame.prefetch(acquire,p,4096);
 }
 check(worldFrame.seal(),"fixture seal");worldFrameReady=true;liveReads=liveRosterReads=missingLiveReads=0;
}
static void reset(){
 ++scenarios;check(!sampleReadDepth,"scope leaked between cases");readMutation={};countWork=false;
 memory.assign(memory.size(),0);::new(static_cast<void*>(sh)) Shared{};std::memset(&snap,0,sizeof snap);
 worldObjects.clear();manualVersions.clear();nextAddress=0x100000;lastHouse=0x20000;++match;clockNow=1000;snap.frame=100;snap.session=3;snap.match=match;
 worldFrameReady=false;liveReads=liveRosterReads=missingLiveReads=nativeCalls=0;missingPage=0;featureConfigRevision=-1;featureConfig={};featureConfigNext={};
 nativeCount=0;oreConfig={};oreConfigRevision=0;planningFrame=-1;planningWorld.count=planningWorld.intentCount=planningWorld.refineryCount=planningWorld.combatCount=0;planningSerial=inputSerial=0;
 std::memset(weaponCache,0,sizeof weaponCache);sh->flags=AntiTheft;
 set<uint32_t>(lastHouse+0x34,0x40000);set<int>(lastHouse+0x30,0);set<uint32_t>(0x30000+0x34,0x50000);set<int>(0x30000+0x30,1);
 vectorAt(0xA8EB40,0x840000,{});vectorAt(0xA8EC78,0x800000,{});vectorAt(0xA83CE0,0x850000,{});
 sampleReads=buildingChecks=0;
}
static uint32_t type(const char* name){const uint32_t p=alloc();std::memcpy(memory.data()+p+0x24,name,std::strlen(name)+1);set<int>(p+0x684,-1);return p;}
static uint32_t building(uint32_t p,uint32_t t,uint32_t id,uint32_t house){
 set<uint32_t>(p,0x7E3EBC);set<uint32_t>(p+0x520,t);set<uint32_t>(p+0x10,id);set<uint32_t>(p+0x21C,house);
 set<int>(p+0x6C,100);set<bool>(p+0x74,true);set<bool>(p+0x90,true);set<int>(p+0x9C,1000);set<int>(p+0xA0,1000);set<int>(p+0xAC,5);set<int>(p+0xB4,-1);worldObjects.add(p,id);return p;
}
static std::vector<uint32_t> buildings(int count,bool eligibleLast=false){
 reset();const uint32_t normal=type("NACNST"),eligible=type("NALASR");std::vector<uint32_t> list;
 for(int n=0;n<count;++n)list.push_back(building(0x300000+uint32_t(n)*0x800,eligibleLast&&n+1==count?eligible:normal,100+n,lastHouse));
 vectorAt(0xA8EB40,0x840000,list);vectorAt(0xA8EC78,0x800000,list);seal();return list;
}
static void filter(const char* name){
 sh->featureConfigSeq|=1;auto& f=sh->featureConfig.filters[featuretypes::AntiTheftDefense];f={};f.custom=1;f.count=1;std::memcpy(f.ids[0],name,std::strlen(name)+1);++sh->featureConfigSeq;
}
static uint64_t digestWorld(){
 uint64_t out=1469598103934665603ull;auto feed=[&](const void* p,size_t n){auto b=static_cast<const unsigned char*>(p);for(size_t i=0;i<n;++i)out=(out^b[i])*1099511628211ull;};
 feed(&planningWorld.count,sizeof planningWorld.count);feed(planningWorld.actors,size_t(planningWorld.count)*sizeof(planner::Actor));return out;
}
static void benchmark(int count,bool positive){
 buildings(count,positive);check(publishPlanningWorld(lastHouse,snap.frame,clockNow)==positive,"initial presence publication");
 const auto semantic=digestWorld();const auto worldSeq=sh->worldSeq;sampleReads=buildingChecks=0;countWork=true;std::vector<long long> times;
 for(int batch=0;batch<5;++batch){const auto start=std::chrono::steady_clock::now();for(int n=0;n<20;++n)check(!publishPlanningWorld(lastHouse,snap.frame,clockNow),"same source unexpectedly republished");
  times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count());}
 countWork=false;std::sort(times.begin(),times.end());check(!liveReads&&!missingLiveReads&&digestWorld()==semantic&&sh->worldSeq==worldSeq,"stable semantic output or sealed boundary changed");
#if ANTI_THEFT_BASELINE
 check(buildingChecks==uint64_t(count)*100,"baseline must execute its full repeated pre-scan");
#else
 check(!buildingChecks&&!sampleReads,"cache hit must not repeat raw roster or predicate reads");
#endif
 std::printf("BENCH {\"buildings\":%d,\"positive\":%s,\"calls\":100,\"candidate_checks\":%llu,\"sealed_reads\":%llu,\"live_reads\":%u,\"median_ns_per_20\":%lld,\"min_ns_per_20\":%lld,\"max_ns_per_20\":%lld,\"active_world_digest\":\"%016llx\"}\n",count,positive?"true":"false",(unsigned long long)buildingChecks,(unsigned long long)sampleReads,liveReads,times[2],times.front(),times.back(),(unsigned long long)semantic);
}
#if !ANTI_THEFT_BASELINE
static bool presence(uint32_t house,bool& present,bool& changed){SampleReadScope scope;if(!scope)return false;long revision;return collectAntiTheftPresence(house,present,changed,revision)&&scope.complete();}
static void identityCases(){
 buildings(4);bool present=false,changed=false;check(presence(lastHouse,present,changed)&&!present&&changed,"cold negative presence");
 for(int field=0;field<16;++field){
  ++scenarios;auto& k=worldFrame.key;
  switch(field){case 0:++k.pid;break;case 1:++k.owner;break;case 2:++k.match;++match;break;case 3:++k.serial;break;case 4:++k.tick;break;case 5:++k.demand;break;case 6:++k.scenario;break;case 7:++k.cells;break;case 8:++k.house;break;case 9:++k.cellCount;break;case 10:++k.frame;++snap.frame;break;case 11:++k.session;break;case 12:++k.generationLow;break;case 13:++k.generationHigh;break;case 14:++k.connectionLow;break;case 15:++k.connectionHigh;break;}
  sampleReads=buildingChecks=0;countWork=true;check(presence(lastHouse,present,changed)&&!present&&changed&&buildingChecks==4,"every full source identity component invalidates presence");
  sampleReads=buildingChecks=0;check(presence(lastHouse,present,changed)&&!present&&!changed&&!sampleReads&&!buildingChecks,"new identity becomes reusable only after complete scan");countWork=false;
 }
 buildings(4);check(presence(lastHouse,present,changed),"demand cold scan");auto demands=runtimeWorldDemands;check(!demands.empty(),"scan must subscribe its raw dependencies");
 check(presence(lastHouse,present,changed)&&runtimeWorldDemands==demands,"same-token reuse must preserve dependencies");seal();check(runtimeWorldDemands.empty(),"new capture starts new consumer demand set");
 check(presence(lastHouse,present,changed)&&changed&&runtimeWorldDemands==demands,"new serial rescans and renews all required dependencies");
}
static void sourceFailures(){
 bool present=false,changed=false;auto list=buildings(2);const uint32_t t=get<uint32_t>(list[0]+0x520);seal(t);
 for(int n=0;n<2;++n){sampleReads=buildingChecks=0;countWork=true;check(!presence(lastHouse,present,changed)&&buildingChecks==2,"missing type data cannot become a cached negative");countWork=false;}
 check(std::find(runtimeWorldDemands.begin(),runtimeWorldDemands.end(),t&~4095u)!=runtimeWorldDemands.end()&&!liveReads,"missing type page remains demanded without live fallback");
 seal();check(presence(lastHouse,present,changed)&&!present&&changed,"complete successor recovers missing data");
 for(bool known:{false,true}){buildings(2);seal(0x840000,known);check(!presence(lastHouse,present,changed)&&!liveReads,"absent or known-failed roster items are unknown");}
 for(int mode=0;mode<4;++mode){buildings(2);Vec v{};std::memcpy(&v,memory.data()+0xA8EB40,sizeof v);if(mode==0)v.count=-1;if(mode==1)v.capacity=1;if(mode==2)v.items=0;if(mode==3){v.count=v.capacity=16385;}
  std::memcpy(memory.data()+0xA8EB40,&v,sizeof v);seal();check(!presence(lastHouse,present,changed),"malformed roster cannot be cached as empty");}
 buildings(0);vectorAt(0xA8EB40,0,{});seal();check(presence(lastHouse,present,changed)&&!present&&changed,"legitimate empty roster supports a negative");
 sampleReads=0;countWork=true;check(presence(lastHouse,present,changed)&&!changed&&!sampleReads,"complete empty roster reuses negative");countWork=false;
 buildings(2,true);check(presence(lastHouse,present,changed)&&present,"valid positive warmup");worldFrameReady=false;check(!presence(lastHouse,present,changed),"not-ready source cannot reuse positive");worldFrameReady=true;
 ++match;check(!presence(lastHouse,present,changed),"mismatched current match rejects source");--match;++snap.frame;check(!presence(lastHouse,present,changed),"mismatched current frame rejects source");--snap.frame;
 const auto key=worldFrame.key;worldFrame.reset(key);check(!presence(lastHouse,present,changed),"unsealed reused object cannot reuse positive");
 buildings(1,true);long revision;check(!collectAntiTheftPresence(lastHouse,present,changed,revision)&&!liveReads,"presence called outside sealed read scope cannot perform live reads");
}
static void positiveAndProjection(){
 auto list=buildings(2);const uint32_t eligible=type("NALASR");set<uint32_t>(list[0]+0x520,eligible);vectorAt(0xA8EB40,0x840ffc,list);seal(0x841000);
 bool present=false,changed=false;check(presence(lastHouse,present,changed)&&present&&changed,"positive proof only needs visited prefix, not unread roster tail");
 check(publishPlanningWorld(lastHouse,snap.frame,clockNow)&&planningWorld.count==1,"complete Techno projection can use that prefix proof");
 list=buildings(2,true);seal(0x800000);const auto seq=sh->worldSeq;check(!publishPlanningWorld(lastHouse,snap.frame,clockNow)&&sh->worldSeq==seq&&planningFrame==-1,"positive presence cannot publish an incomplete World");
 sampleReads=buildingChecks=0;countWork=true;check(!publishPlanningWorld(lastHouse,snap.frame,clockNow)&&sampleReads>0&&!buildingChecks&&planningFrame==-1,"cached positive must continue missing projection reads");countWork=false;
 check(std::find(runtimeWorldDemands.begin(),runtimeWorldDemands.end(),0x800000)!=runtimeWorldDemands.end()&&!liveReads,"projection demand survives cached positive");
 seal();check(publishPlanningWorld(lastHouse,snap.frame,clockNow)&&planningWorld.count==1,"complete successor publishes without manual intervention");
}
static void sameFrameChanges(){
 auto list=buildings(2);const uint32_t other=type("NAPOWR");set<uint32_t>(list[1]+0x520,other);seal();filter("NACNST");
 check(publishPlanningWorld(lastHouse,snap.frame,clockNow)&&planningWorld.count==1&&planningWorld.actors[0].id==100,"first custom tower participates");
 filter("NAPOWR");check(publishPlanningWorld(lastHouse,snap.frame,clockNow)&&planningWorld.count==1&&planningWorld.actors[0].id==101,"same-frame positive-to-positive filter change republishes participants");
 filter("NONE");check(publishPlanningWorld(lastHouse,snap.frame,clockNow)&&!planningWorld.count,"positive-to-negative change clears old defense actors");
 list=buildings(2);const uint32_t eligible=type("NALASR");set<uint32_t>(list[0]+0x520,eligible);set<uint32_t>(list[1]+0x520,eligible);set<uint32_t>(list[1]+0x21C,0x30000);seal();
 check(publishPlanningWorld(lastHouse,snap.frame,clockNow)&&planningWorld.actors[0].id==100,"first house publication");
 check(publishPlanningWorld(0x30000,snap.frame,clockNow)&&planningWorld.count==1&&planningWorld.actors[0].id==101,"same source with different own house republishes correct actor");
 set<int>(list[1]+0x9C,1777);seal();check(publishPlanningWorld(0x30000,snap.frame,clockNow)&&planningWorld.actors[0].x==1777,"same-frame new source serial republishes current actor facts");
 list=buildings(2);sh->flags|=AdvancedInput;nativeOrders[0]={100,0,1,0,false,list[0]};nativeCount=1;
 check(publishPlanningWorld(lastHouse,snap.frame,clockNow)&&planningWorld.count==1,"other-feature same-frame gate setup");const auto seq=sh->worldSeq;filter("NO_TOWERS");
 check(!publishPlanningWorld(lastHouse,snap.frame,clockNow)&&sh->worldSeq==seq&&planningFrame==snap.frame,"negative-to-negative AntiTheft config must preserve other feature frame gate");
 ++inputSerial;check(publishPlanningWorld(lastHouse,snap.frame,clockNow),"existing manual input serial still bypasses same-frame gate");
}
static void configFailures(){
 auto list=buildings(2);bool present=false,changed=false;check(presence(lastHouse,present,changed)&&!present,"negative config setup");
 sh->featureConfigSeq=1;check(!presence(lastHouse,present,changed),"odd feature sequence cannot reuse cached negative");filter("NACNST");check(presence(lastHouse,present,changed)&&present&&changed,"new stable config is immediate on same source");
 sh->featureConfigSeq=4;sh->featureConfig.filters[featuretypes::AntiTheftDefense].custom=2;check(!presence(lastHouse,present,changed),"invalid complete feature config cannot be memoized");
 list=buildings(2);const uint32_t t=get<uint32_t>(list[0]+0x520);seal();readMutation=[&](uint32_t at,size_t){if(at==t+0x24){readMutation={};filter("NACNST");}};
 check(!presence(lastHouse,present,changed),"config changing during pre-scan cannot store a mixed result");check(presence(lastHouse,present,changed)&&present&&changed,"pre-scan retries a newly coherent config");
 list=buildings(2,true);check(publishPlanningWorld(lastHouse,snap.frame,clockNow),"projection coherence setup");const auto seq=sh->worldSeq;const auto world=digestWorld();++inputSerial;
 readMutation=[&](uint32_t at,size_t){if(at==list.back()+0x9C){readMutation={};filter("NACNST");}};
 check(!publishPlanningWorld(lastHouse,snap.frame,clockNow)&&sh->worldSeq==seq&&digestWorld()==world,"config change during later World projection preserves old publication");
 check(publishPlanningWorld(lastHouse,snap.frame,clockNow)&&planningWorld.count==1&&planningWorld.actors[0].id==100,"projection retries newly coherent participant config");
 list=buildings(2);const auto atType=get<uint32_t>(list[0]+0x520);seal();readMutation=[&](uint32_t at,size_t){if(at==atType+0x24){readMutation={};++worldFrame.key.serial;}};
 check(!presence(lastHouse,present,changed),"source changing during pre-scan is not memoized");check(presence(lastHouse,present,changed)&&!present&&changed,"coherent successor after source change is rescanned");check(!liveReads&&!missingLiveReads,"failure tests stay sealed");
}
#endif
int main(){sh=new Shared{};benchmark(0,false);benchmark(256,false);benchmark(1024,false);benchmark(1024,true);
#if !ANTI_THEFT_BASELINE
 identityCases();sourceFailures();positiveAndProjection();sameFrameChanges();configFailures();
#endif
 std::printf("RESULT {\"checks\":%u,\"scenarios\":%u,\"failures\":0}\n",checks,scenarios);delete sh;}
