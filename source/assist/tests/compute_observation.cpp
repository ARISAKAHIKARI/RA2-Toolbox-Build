#include "compute_frame.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
#ifdef MEMCPY_OBSERVATION
static bool countCopies=false;static size_t copied=0;
extern "C" void*__real_memcpy(void*,const void*,size_t);
extern "C" void*__wrap_memcpy(void*to,const void*from,size_t n){if(countCopies)copied+=n;return __real_memcpy(to,from,n);}
#endif
static bool track=false;static size_t allocated=0,largest=0;
void*operator new(size_t n){if(track){allocated+=n;if(n>largest)largest=n;}if(void*p=std::malloc(n))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}
using namespace assist;
int main(){
 auto load=std::make_unique<compute::LoadInput>();auto missile=std::make_unique<missilecompute::Input>();
 compute::LoadObservation lo;missilecompute::Observation mo;
 load->match=missile->match=9;load->tick=missile->tick=1234;load->frame=missile->frame=200;
 load->ns=loading::MaxInfantry;load->nv=loading::MaxVehicles;load->mode=1;load->radius=12;
 missile->count=missilecompute::MaxSources;missile->targetCount=missilecompute::MaxTargets;missile->mode=3;
 for(int i=0;i<load->ns;++i){load->soldiers[i].id=100+i;load->soldiers[i].object=0x100000+i*4096;load->tokens[i]=i+4;}
 for(int i=0;i<load->nv;++i){load->vehicles[i].id=2000+i;load->vehicles[i].capacity=4;}
 for(int i=0;i<missile->count;++i){missile->sources[i].id=3000+i;missile->sources[i].pointer=0x200000+i*4096;missile->sources[i].token=7;}
 for(int i=0;i<missile->targetCount;++i){missile->targets[i].id=4000+i;missile->targets[i].hp=500;}
 compute::observe(lo,*load);missilecompute::observe(mo,*missile);
 assert(compute::valid(lo)&&lo.tick==1234&&lo.frame==200&&lo.mode==1&&lo.radius==12);
 assert(!std::memcmp(lo.soldiers,load->soldiers,sizeof lo.soldiers)&&!std::memcmp(lo.vehicles,load->vehicles,sizeof lo.vehicles)&&!std::memcmp(lo.tokens,load->tokens,sizeof lo.tokens));
 assert(mo.match==9&&mo.tick==1234&&mo.frame==200&&mo.mode==3);
 assert(!std::memcmp(mo.sources,missile->sources,sizeof mo.sources)&&!std::memcmp(mo.targets,missile->targets,sizeof mo.targets));
 compute::FrameHistory<compute::LoadObservation>loads;compute::FrameHistory<missilecompute::Observation>missiles;
 track=true;for(uint32_t serial=1;serial<=16;++serial){loads.retain(serial,lo);missiles.retain(serial,mo,{1,2,3});}track=false;
 const size_t firstBytes=allocated;assert(firstBytes<2*1024*1024&&largest<100*1024);
 load->soldiers[0].id=90000;load->tick=9999;missile->sources[0].id=90000;missile->tick=9999;
 assert(loads.find(1)->soldiers[0].id==100&&loads.find(1)->tick==1234&&missiles.find(1)->sources[0].id==3000&&missiles.find(1)->tick==1234);
 track=true;for(uint32_t serial=17;serial<=1000;++serial){loads.retain(serial,lo);missiles.retain(serial,mo,{1,2,3});}track=false;
 assert(allocated==firstBytes&&!loads.find(1)&&!missiles.find(1)&&loads.find(1000)&&missiles.cursors(1000)->at(2)==3);
 assert(sizeof(lo)*5<sizeof(*load)&&sizeof(mo)*30<sizeof(*missile));
#ifdef MEMCPY_OBSERVATION
 lo.ns=lo.nv=mo.count=mo.targetCount=1;countCopies=true;loads.retain(1001,lo);missiles.retain(1001,mo);countCopies=false;
 const size_t bytes=offsetof(compute::LoadObservation,soldiers)+sizeof(loading::Infantry)+sizeof(loading::Vehicle)+4+offsetof(missilecompute::Observation,sources)+sizeof(missilecompute::Source)+sizeof(missilecompute::Target);
 assert(copied==bytes);printf("PASS counted execution observations: one source/vehicle and one shooter/target copy only %zu bytes together\n",copied);
#endif

 const auto original=lo.soldiers[0].id;load->ns=loading::MaxInfantry+1;compute::observe(lo,*load);assert(!compute::valid(lo)&&lo.soldiers[0].id==original);
 missile->count=missilecompute::MaxSources+1;missilecompute::observe(mo,*missile);assert(mo.count==missilecompute::MaxSources+1&&mo.sources[0].id==3000);
 printf("PASS execution observations: load %zu -> %zu bytes; missile %zu -> %zu bytes; 16+16 histories allocate %zu bytes, max %zu; no allocations during 984 subsequent retains; source IDs/tokens/timestamps and bounds preserved\n",sizeof(*load),sizeof(compute::LoadObservation),sizeof(*missile),sizeof(missilecompute::Observation),firstBytes,largest);
}
