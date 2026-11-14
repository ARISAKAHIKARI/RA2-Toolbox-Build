#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_set>
#include <vector>
#include "reverse_dodge.h"
#include "reverse_target_index.h"
#include "dodge_compute.h"
using namespace assist;
using DWORD=uint32_t;
struct DodgeThreatRef {uint32_t pointer,target;bool projectile;uint32_t id;};
static DodgeThreatRef dodgeThreatRefs[8192];static int dodgeThreatCount;
struct DodgeJob {uint32_t pointer=0,id=0;bool active=false;};
static DodgeJob dodgeJobs[512];static int dodgeCount;
static dodgecompute::Input dodgeCapture,dodgeInput;
static struct {unsigned dodgeActive=0;long featureConfigSeq=2;} shared;
static auto*sh=&shared;
static uint32_t match=1;
static std::vector<uint8_t> memory(8*1024*1024);
static std::vector<uint32_t> units;
static std::unordered_set<uint32_t> members;
static struct {bool contains(uint32_t p)const{return members.count(p)!=0;}}worldObjects;
static int currentFrame=100;
static struct {struct {uint32_t serial=1;}key;}worldFrame;static uint32_t featureConfigRevision=1;
static bool sampleReady=true,sampleComplete=true,actorAccept=true,sourceEnabled=true,queueReady=true;
static bool&sampleReadComplete=sampleComplete;
static uint32_t actorAttempts=0,continuations=0,observations=0;
static uint64_t rosterVisits=0,readCount=0;
static uint32_t checks=0,cases=0,semanticDigest=2166136261u;
static constexpr uint32_t Unit=0x100000,Bullet=0x500000,Cell=0x700000;
template<class T>static void set(uint32_t at,T value){if(uint64_t(at)+sizeof(T)>memory.size())std::abort();std::memcpy(memory.data()+at,&value,sizeof value);}
template<class T>static T get(uint32_t at){T value{};++readCount;if(uint64_t(at)+sizeof value<=memory.size())std::memcpy(&value,memory.data()+at,sizeof value);else sampleComplete=false;return value;}
static bool read(uint32_t at,void*out,size_t bytes){++readCount;if(uint64_t(at)+bytes>memory.size()){sampleComplete=false;return false;}std::memcpy(out,memory.data()+at,bytes);return true;}
static int kind(uint32_t p){return worldObjects.contains(p)?2:-1;}
struct Vec{int count;};
static bool sampleRoster(uint32_t,Vec&v){v.count=int(units.size());return sampleReady;}
static uint32_t item(Vec,int i){++rosterVisits;return units[size_t(i)];}
class SampleReadScope {public:explicit operator bool()const{return sampleReady;}bool complete()const{return sampleReady&&sampleComplete;}};
static uint32_t readMapCell(int x,int y){return x==40&&y==40?Cell:0;}
static uint32_t worldByID(uint32_t id){for(auto p:units)if(get<uint32_t>(p+0x10)==id)return p;return 0;}
static bool dodgeSource(uint32_t p,uint32_t){return sourceEnabled&&worldObjects.contains(p);}
static int dodgeThreat(uint32_t,uint32_t,uint32_t){return 1;}
static void dodgeObserve(uint32_t p,uint32_t,dodgecompute::Source&s){++observations;s.pointer=p;s.id=get<uint32_t>(p+0x10);s.frame=currentFrame;s.count=1;s.freshThreat=1;for(int n=0;n<dodgeCount;++n)if(dodgeJobs[n].pointer==p)s.active=dodgeJobs[n].active;}
static bool outgoingRoom(){return queueReady;}
static bool reverseDodgeActor(uint32_t,uint32_t,uint32_t,int,DWORD,reversedodge::Point){++actorAttempts;return actorAccept&&queueReady;}
static void applyDodgeResult(const dodgecompute::Result&,uint32_t,int,DWORD){++continuations;}
static void dispatchDodgeCalculation(uint32_t h,int f,DWORD n,int,int,int){applyDodgeResult(dodgecompute::calculate(dodgeInput),h,f,n);}
#include "reverse_hotpath_production.h"
static void check(bool ok,const char*message){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static void seal(uint32_t extra=0){++cases;for(uint32_t v:{actorAttempts,continuations,observations,sh->dodgeActive,extra})semanticDigest=(semanticDigest^v)*16777619u;}
static void reset(int count=4){
 std::fill(memory.begin(),memory.end(),uint8_t(0));units.clear();members.clear();std::memset(reverseShots,0,sizeof reverseShots);
 ++worldFrame.key.serial;reverseMatch=match=1;reverseWasEnabled=false;dodgeThreatCount=dodgeCount=0;shared={};
 currentFrame=100;sampleReady=sampleComplete=actorAccept=sourceEnabled=queueReady=true;actorAttempts=continuations=observations=0;rosterVisits=readCount=0;
 for(int n=0;n<count;++n){uint32_t p=Unit+uint32_t(n)*0x400;units.push_back(p);members.insert(p);set<uint32_t>(p+0x10,1000+uint32_t(n));set<int>(p+0x9C,(40+n)*256);set<int>(p+0xA0,40*256);}
}
static void addBullet(unsigned n,uint32_t target=Unit,uint32_t id=700){
 uint32_t p=Bullet+n*0x400;set<uint32_t>(p,0x7E46E4);set<uint32_t>(p+0x10,id);set<uint8_t>(p+0x81,0);set<uint8_t>(p+0x90,1);set<int>(p+0x6C,10);
 set<double>(p+0xE8,20.);set<double>(p+0xF0,0.);set<uint32_t>(p+0x10C,target);
 dodgeThreatRefs[dodgeThreatCount++]={p,target,true,id};
}
static void tick(int frame=100){if(frame!=currentFrame)++worldFrame.key.serial;currentFrame=frame;check(reverseDodgeTick(0x600000,frame,1000+uint32_t(frame))==sampleComplete,"continuation completion");}
static uint32_t tableDigest(){uint32_t h=2166136261u;for(const auto&s:reverseShots)for(uint32_t v:{s.id,s.pointer,uint32_t(s.frame),uint32_t(s.handled)})h=(h^v)*16777619u;return h;}
int main(){
 reset();addBullet(0);tick();check(actorAttempts==1,"initial direct target");for(int n=0;n<99;++n)tick();check(actorAttempts==1&&continuations==100,"handled direct shot or ongoing maintenance repeated");seal(tableDigest());
 reset(512);for(unsigned n=0;n<64;++n)addBullet(n,Cell,700+n);dodgeCount=1;dodgeJobs[0]={Unit,1000,true};tick();
 const auto firstVisits=rosterVisits;const auto firstReads=readCount;for(int n=0;n<99;++n)tick();
 const auto repeatVisits=rosterVisits-firstVisits;const auto repeatReads=readCount-firstReads;
 const auto stressActors=actorAttempts;const auto stressContinuations=continuations;
 check(actorAttempts==64&&continuations==100&&observations==100&&sh->dodgeActive==1,"processed salvo must preserve continuing jobs");seal(tableDigest());
 reset();addBullet(0);actorAccept=false;tick();check(actorAttempts==1&&!reverseShot(Bullet,100)->handled,"failed admission remains pending");actorAccept=true;tick();tick();check(actorAttempts==2&&reverseShot(Bullet,100)->handled,"same-frame retry remains live");seal(tableDigest());
 reset();addBullet(0);actorAccept=false;tick();actorAccept=true;tick(102);check(actorAttempts==2,"later-frame retry before expiry");seal(tableDigest());
 reset();addBullet(0);tick();set<uint32_t>(Bullet+0x10,701);dodgeThreatRefs[0].id=701;tick();check(actorAttempts==2,"new UniqueID at same pointer");seal(tableDigest());
 reset();addBullet(0);tick();set<uint32_t>(Bullet+0x10,701);tick();check(actorAttempts==1,"stale sampled projectile must not issue");dodgeThreatRefs[0].id=701;tick();check(actorAttempts==2,"later fresh identity must issue");seal(tableDigest());
 reset();addBullet(0);tick();++match;tick();check(actorAttempts==2,"world generation reset");seal(tableDigest());
 reset();addBullet(0);tick();reverseWasEnabled=false;tick();check(actorAttempts==2,"reenable reset");seal(tableDigest());
 reset();addBullet(0);const auto empty=tableDigest();sampleComplete=false;currentFrame=100;check(!reverseDodgeTick(0x600000,100,1100),"incomplete raw sample");check(tableDigest()==empty&&actorAttempts==0,"failed collection must not create or age a shot");sampleComplete=true;tick(109);check(actorAttempts==1&&reverseShot(Bullet,109)->frame==109,"expiry starts only after first successful collection");seal(tableDigest());
 reset();addBullet(0);actorAccept=false;tick();actorAccept=true;tick(109);check(actorAttempts==1&&reverseShot(Bullet,109)->handled,"pending shot expires after eight frames");tick(110);check(actorAttempts==1,"expired handled shot stays suppressed");seal(tableDigest());
 reset();addBullet(0);set<double>(Bullet+0xE8,0.);set<double>(Bullet+0xF0,0.);tick();tick();check(actorAttempts==0&&reverseShot(Bullet,100)->handled,"no vector marks the shot handled once");seal(tableDigest());
 reset();addBullet(0);tick();tick(1001);check(actorAttempts==1,"same exact handled record remains suppressed at age 901");seal(tableDigest());
 reset();addBullet(0);reverseWasEnabled=true;const unsigned slot=(700u*2654435761u)&16383u;
 reverseShots[slot]={700+16384,Bullet+0x10000,-1000,false};reverseShots[(slot+1)&16383u]={700,Bullet,100,true};
 tick();check(actorAttempts==1&&reverseShots[slot].id==700,"earlier reusable collision slot preserves original insertion order");seal(tableDigest());
 reset();addBullet(0);reverseWasEnabled=true;
 for(unsigned n=0;n<16384;++n)reverseShots[n]={100000+n,0x600000+n*4,100,false};const auto full=tableDigest();
 tick();check(actorAttempts==0&&tableDigest()==full,"full table must not evict a pending record");seal(tableDigest());
 reset();addBullet(0);tick();addBullet(1,Unit,700);tick();check(actorAttempts==2,"same UniqueID with a different pointer is not the same shot");seal(tableDigest());
 reset();addBullet(0);set<uint32_t>(Bullet+0x10C,units[1]);tick();check(actorAttempts==0&&!reverseShot(Bullet,100)->handled,"changed current target defers the sampled candidate");dodgeThreatRefs[0].target=units[1];tick();check(actorAttempts==1,"updated target may retry in same frame");seal(tableDigest());
 reset();addBullet(0);dodgeThreatRefs[dodgeThreatCount++]={units[1],Unit,false,1001};dodgeCount=1;dodgeJobs[0]={Unit,1000,true};tick();tick();check(actorAttempts==1&&continuations==2&&sh->dodgeActive==1&&observations>=2,"nonprojectile and active-job input remain serviced");seal(tableDigest());
 reset();addBullet(0,Cell);sourceEnabled=false;tick();sourceEnabled=true;tick();check(actorAttempts==0,"completed no-candidate shot stays handled under existing contract");seal(tableDigest());
#ifdef REVERSE_QUERY_BUDGET_GUARD
 reset();addBullet(0);queueReady=false;tick();check(!actorAttempts&&!reverseShot(Bullet,100)->handled&&reverseShot(Bullet,100)->frame==100,"full queue ran native placement or changed deadline");queueReady=true;tick(101);check(actorAttempts==1&&reverseShot(Bullet,101)->handled,"pending movement did not recover after room returned");
 reset();addBullet(0);queueReady=false;tick();queueReady=true;tick(109);check(!actorAttempts&&reverseShot(Bullet,109)->handled,"queue wait renewed original pending deadline");
#endif
 std::printf("{\"pass\":true,\"checks\":%u,\"cases\":%u,\"semantic_digest\":%u,\"first_roster_visits\":%llu,\"repeated_roster_visits\":%llu,\"first_reads\":%llu,\"repeated_reads\":%llu,\"stress_actor_attempts\":%u,\"continuation_passes\":%u}\n",checks,cases,semanticDigest,(unsigned long long)firstVisits,(unsigned long long)repeatVisits,(unsigned long long)firstReads,(unsigned long long)repeatReads,stressActors,stressContinuations);
}
