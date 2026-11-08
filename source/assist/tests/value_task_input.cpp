#include "../source/protocol.h"
#include "../source/value_task.h"
#include BASELINE_VALUE_TASK
#include BASELINE_FRAME_HISTORY
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <stdexcept>
#include <type_traits>
using namespace assist;

// Link-time memcpy wrapping is invisible to the optimizer at each call site.
// Volatile counters keep the measurement window and its inputs observable.
static thread_local volatile bool measuring=false,failInputAllocation=false;
static thread_local volatile size_t inputSize=0;
static thread_local volatile uintptr_t inputBegin=0,inputEnd=0;
static thread_local volatile uint64_t allocations=0,allocationBytes=0,largeAllocations=0,largeAllocationBytes=0,copiedBytes=0;
void*operator new(size_t n){
 if(failInputAllocation&&n==inputSize){failInputAllocation=false;throw std::bad_alloc();}
 if(void*p=std::malloc(n)){if(measuring){++allocations;allocationBytes+=n;if(n>=inputSize&&n<=inputSize+128){++largeAllocations;largeAllocationBytes+=n;}}return p;}throw std::bad_alloc();
}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}
extern "C" void*__real_memcpy(void*,const void*,size_t);
extern "C" void*__wrap_memcpy(void*d,const void*s,size_t n){
 const auto at=reinterpret_cast<uintptr_t>(s);
 if(measuring&&at>=inputBegin&&at<=inputEnd&&n<=inputEnd-at)copiedBytes+=n;
 return __real_memcpy(d,s,n);
}
static unsigned checks=0,scenarios=0;
static void check(bool value,const char*reason){++checks;if(!value)throw std::runtime_error(reason);}
static void watch(const void*source,size_t bytes){inputSize=bytes;inputBegin=reinterpret_cast<uintptr_t>(source);inputEnd=inputBegin+bytes;allocations=allocationBytes=largeAllocations=largeAllocationBytes=copiedBytes=0;}
static Shared*shared,*sh;static Snapshot snap;static uint64_t planningGeneration=1;static uint32_t clockNow=1000;
static unsigned barriers=0,wakes=0;static std::function<void(unsigned)>barrierHook;
static uint32_t GetTickCount(){return clockNow;}
static void MemoryBarrier(){std::atomic_thread_fence(std::memory_order_seq_cst);++barriers;if(barrierHook)barrierHook(barriers);}
template<class T>static void InterlockedIncrement(volatile T*p){++*p;}
static void wakeGame(){++wakes;}
static void notifyHost(){++wakes;}
#include CURRENT_SERVICE
#include BASELINE_SERVICE

static bool externalInputFrame=true,unloadCalculationPending=false;
static unloadcompute::Input unloadExternalInput;
static uint32_t unloadCalculationSerial=0;
static compute::FrameHistory<unloadcompute::Input>unloadFrames;
static baselinehistory::FrameHistory<unloadcompute::Input>baselineUnloadFrames;
#include CURRENT_UNLOAD_PUBLISH
#include BASELINE_UNLOAD_PUBLISH

template<class Input>static void same(const Input&a,const Input&b){check(!std::memcmp(&a,&b,sizeof(Input)),"full value copies differ");}
static void same(const unloadcompute::Input&a,const unloadcompute::Input&b){
 check(!std::memcmp(&a,&b,offsetof(unloadcompute::Input,sources)),"unload headers differ");
 if(!unloadcompute::valid(a)){check(!std::memcmp(&a,&b,sizeof(a)),"invalid unload falls back to a bounded complete copy");return;}
 check(!std::memcmp(a.sources,b.sources,size_t(a.count)*sizeof(a.sources[0])),"unload source prefix differs");
 check(!std::memcmp(a.targets,b.targets,size_t(a.targetCount)*sizeof(a.targets[0])),"unload target prefix differs");
 for(int n=0;n<a.count;++n)check(!std::memcmp(a.actions[n],b.actions[n],size_t(a.targetCount)),"unload action row differs");
}
static void same(const missilecompute::Input&a,const missilecompute::Input&b){
 check(!std::memcmp(&a,&b,offsetof(missilecompute::Input,sources)),"missile headers differ");
 if(a.count<0||a.count>missilecompute::MaxSources||a.targetCount<0||a.targetCount>missilecompute::MaxTargets){check(!std::memcmp(&a,&b,sizeof(a)),"invalid missile falls back to a bounded complete copy");return;}
 check(!std::memcmp(a.sources,b.sources,size_t(a.count)*sizeof(a.sources[0])),"missile source prefix differs");
 check(!std::memcmp(a.targets,b.targets,size_t(a.targetCount)*sizeof(a.targets[0])),"missile target prefix differs");
 for(int n=0;n<a.count;++n)check(!std::memcmp(a.edges[n],b.edges[n],size_t(a.targetCount)*sizeof(a.edges[n][0])),"missile edge defaults or values differ");
}
static void same(const compute::LoadInput&a,const compute::LoadInput&b){
 check(!std::memcmp(&a,&b,offsetof(compute::LoadInput,soldiers)),"load headers differ");
 if(!compute::valid(a)){check(!std::memcmp(&a,&b,sizeof(a)),"invalid load falls back to a bounded complete copy");return;}
 check(!std::memcmp(a.soldiers,b.soldiers,size_t(a.ns)*sizeof(a.soldiers[0])),"load soldier prefix differs");
 check(!std::memcmp(a.vehicles,b.vehicles,size_t(a.nv)*sizeof(a.vehicles[0])),"load vehicle prefix differs");
 check(!std::memcmp(a.tokens,b.tokens,size_t(a.ns)*sizeof(a.tokens[0])),"load manual tokens differ");
 check(!std::memcmp(&a.composition,&b.composition,offsetof(loading::Composition,numbers)),"load composition types/count differ");
 for(int n=0;n<a.nv;++n)check(!std::memcmp(a.composition.numbers[n],b.composition.numbers[n],sizeof(a.composition.numbers[n])),"load existing and new-type row values differ");
 for(int n=0;n<a.ns;++n)check(!std::memcmp(a.compatible[n],b.compatible[n],size_t(a.nv)),"load compatibility row differs");
}
static size_t payload(const unloadcompute::Input&i){return offsetof(unloadcompute::Input,sources)+size_t(i.count)*sizeof(i.sources[0])+size_t(i.targetCount)*sizeof(i.targets[0])+size_t(i.count)*i.targetCount;}
static size_t payload(const missilecompute::Input&i){return offsetof(missilecompute::Input,sources)+size_t(i.count)*sizeof(i.sources[0])+size_t(i.targetCount)*sizeof(i.targets[0])+size_t(i.count)*i.targetCount*sizeof(i.edges[0][0]);}
static size_t payload(const compute::LoadInput&i){return offsetof(compute::LoadInput,soldiers)+size_t(i.ns)*(sizeof(i.soldiers[0])+sizeof(i.tokens[0]))+size_t(i.nv)*sizeof(i.vehicles[0])+offsetof(loading::Composition,numbers)+size_t(i.nv)*sizeof(i.composition.numbers[0])+size_t(i.ns)*i.nv;}
static void fill(unloadcompute::Input&i,int ns,int nt,unsigned salt=0){
 i.match=9;i.tick=clockNow;i.frame=10;i.count=ns;i.targetCount=nt;
 for(int n=0;n<ns;++n){auto&s=i.sources[n];s={};s.id=100+n;s.job=200+n;s.token=salt;s.view.frame=10;s.view.mission=5;s.state.stableSince=1;}
 for(int t=0;t<nt;++t)i.targets[t]={unsigned(1000+t),unsigned(2000+t),t*16,t*8};
 for(int n=0;n<ns;++n)for(int t=0;t<nt;++t)i.actions[n][t]=uint8_t((n+t+salt)%3?2:0);
}
static void fill(missilecompute::Input&i,int ns,int nt,unsigned salt=0){
 i.match=9;i.tick=clockNow;i.frame=10;i.count=ns;i.targetCount=nt;i.mode=int(salt%4);
 for(int n=0;n<ns;++n){auto&s=i.sources[n];s={};s.id=100+n;s.token=salt;s.cooldown=n%5;s.held=n%2;s.pendingMission=n%7?-1:missilecompute::TransientBusy;}
 for(int t=0;t<nt;++t)i.targets[t]={unsigned(1000+t),unsigned(2000+t),1000+t,unsigned(t%3)};
 for(int n=0;n<ns;++n)for(int t=0;t<nt;++t){auto&e=i.edges[n][t];e={};if((n+t+salt)%3)e={20+(n+t)%7,unsigned(n+t+salt),n%4,t%2};}
}
static void fill(compute::LoadInput&i,int ns,int nv,unsigned salt=0){
 i.match=9;i.tick=clockNow;i.frame=10;i.ns=ns;i.nv=nv;i.mode=int(salt%2);i.radius=30;i.composition.clear();
 i.composition.count=1;i.composition.types[0]=999;
 for(int n=0;n<ns;++n){i.soldiers[n]={unsigned(100+n),unsigned(100+n),n*4,n*8,1,unsigned(1000+(n+salt)%5),1};i.tokens[n]=salt+n;}
 for(int v=0;v<nv;++v){i.vehicles[v]={unsigned(2000+v),unsigned(2000+v),v*12,v*6,8,1,0,8.,1};i.composition.numbers[v][0]=1;}
 for(int n=0;n<ns;++n)for(int v=0;v<nv;++v)i.compatible[n][v]=uint8_t((n+v+salt)%4!=0);
}
static unloadcompute::Result calculate(unloadcompute::Input&i){return unloadcompute::calculate(i);}
static missilecompute::Result calculate(missilecompute::Input&i){return missilecompute::calculate(i);}
static compute::LoadResult calculate(compute::LoadInput&i){thread_local auto work=std::make_unique<loading::Workspace>();return compute::load(i,*work);}
template<class Input>static void transferCases(const char*name,int maxSources,int maxTargets){
 auto source=std::make_unique<Input>();auto target=std::make_unique<Input>();
 const std::pair<int,int>shapes[]={{0,0},{1,1},{3,7},{17,2},{1,maxTargets},{maxSources,1},{maxSources,maxTargets},{2,3},{0,4}};
 for(auto shape:shapes){++scenarios;fill(*source,shape.first,shape.second,unsigned(scenarios));std::memset(target.get(),0x5a,sizeof(Input));watch(source.get(),sizeof(Input));measuring=true;compute::copyInput(*target,*source);measuring=false;
  if(copiedBytes!=payload(*source))std::fprintf(stderr,"copy type=%s shape=%d/%d observed=%llu expected=%zu\n",name,shape.first,shape.second,(unsigned long long)copiedBytes,payload(*source));
  check(copiedBytes==payload(*source),"counted input-copy bytes differ from active payload");check(!allocations,"copyInput must not allocate");same(*target,*source);
  if(shape.first<=17&&shape.second<=7){auto reference=std::make_unique<Input>(*source);auto expected=calculate(*reference);auto actual=calculate(*target);check(!std::memcmp(&expected,&actual,sizeof(expected)),"actual calculator result differs from complete-copy baseline");same(*target,*reference);}
 }
 // Negative/oversized producer counters never become unchecked byte lengths.
 for(bool negative:{false,true}){++scenarios;fill(*source,2,3);if constexpr(std::is_same_v<Input,compute::LoadInput>)source->ns=negative?-1:maxSources+1;else source->count=negative?-1:maxSources+1;
  std::memset(target.get(),0x3c,sizeof(Input));compute::copyInput(*target,*source);same(*target,*source);
  fill(*source,2,3);if constexpr(std::is_same_v<Input,compute::LoadInput>)source->nv=negative?-1:maxTargets+1;else source->targetCount=negative?-1:maxTargets+1;
  compute::copyInput(*target,*source);same(*target,*source);
 }
 std::printf("TRANSFER {\"type\":\"%s\",\"cases\":11,\"input_bytes\":%zu}\n",name,sizeof(Input));
}
template<bool Baseline,class Input,class Output>using Worker=std::conditional_t<Baseline,baselinecompute::ValueTask<Input,Output>,compute::ValueTask<Input,Output>>;
template<bool Baseline,class Input,class Output>static void service(Worker<Baseline,Input,Output>&worker,volatile uint32_t&request,volatile uint32_t&computed,const Input&source,Output&destination,volatile long*sequence=nullptr){
 if constexpr(Baseline)serviceValueTaskBaseline(worker,request,computed,source,destination,sequence);
 else serviceValueTask(worker,request,computed,source,destination,sequence);
}
template<bool Baseline,class Input,class Output>static void hostMeasurement(const char*name,int ns,int nt){
 ++scenarios;auto source=std::make_unique<Input>();auto destination=std::make_unique<Output>();fill(*source,ns,nt);
 snap.match=9;snap.frame=10;shared->computeFrame=10;shared->computeFrameMatch=9;shared->computeFrameSeq=2;planningGeneration=1;
 std::mutex mutex;std::condition_variable cv;bool entered=false,release=false;std::atomic<unsigned>executed{0};bool pinnedSame=false;
 Worker<Baseline,Input,Output>worker([&](Input&i){
  if(executed.fetch_add(1)==0){auto original=std::make_unique<Input>(i);{std::unique_lock<std::mutex>lock(mutex);entered=true;cv.notify_one();cv.wait(lock,[&]{return release;});}pinnedSame=std::memcmp(original.get(),&i,sizeof i)==0;}
  return calculate(i);
 });
 volatile uint32_t request=1,computed=0;volatile long seq=2;
 service<Baseline>(worker,request,computed,*source,*destination,&seq);
 {std::unique_lock<std::mutex>lock(mutex);check(cv.wait_for(lock,std::chrono::seconds(5),[&]{return entered;}),"worker did not enter the controlled calculation");}
 for(int n=0;n<5;++n){++request;service<Baseline>(worker,request,computed,*source,*destination,&seq);}
 watch(source.get(),sizeof(Input));std::array<long long,5>times{};measuring=true;
 for(auto&elapsed:times){const auto begin=std::chrono::steady_clock::now();for(int n=0;n<100;++n){++request;service<Baseline>(worker,request,computed,*source,*destination,&seq);}elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count();}
 measuring=false;const uint64_t count=allocations,bytes=allocationBytes,large=largeAllocations,largeBytes=largeAllocationBytes,copies=copiedBytes;
 check(large==(Baseline?500:0),"warm Host path large input allocation count differs");
 check(count==(Baseline?1000:500),"Host small Request allocation accounting differs");
 check(copies==500*(Baseline?sizeof(Input):payload(*source)),"actual Host captured byte count differs");
 {std::lock_guard<std::mutex>lock(mutex);release=true;cv.notify_one();}
 const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(10);
 std::shared_ptr<const typename Worker<Baseline,Input,Output>::Result>result;
 do{result=worker.get();if(result&&result->task==request)break;std::this_thread::yield();}while(std::chrono::steady_clock::now()<limit);
 check(result&&result->task==request,"newest pending input did not finish");check(pinnedSame,"a running input was modified or recycled");check(executed==2,"running plus newest-only pending execution semantics changed");
 service<Baseline>(worker,request,computed,*source,*destination,&seq);check(computed==request,"actual Host publication did not acknowledge the original source serial");
 auto reference=std::make_unique<Input>(*source);auto expected=calculate(*reference);check(!std::memcmp(destination.get(),&expected,sizeof expected),"Host completed output differs from full source calculation");
 std::sort(times.begin(),times.end());std::printf("HOST {\"variant\":\"%s\",\"type\":\"%s\",\"sources\":%d,\"targets\":%d,\"input_bytes\":%zu,\"calls\":500,\"input_allocations\":%llu,\"input_allocation_bytes\":%llu,\"main_thread_allocations\":%llu,\"main_thread_allocation_bytes\":%llu,\"request_allocations\":500,\"copied_bytes\":%llu,\"median_ns_per_100\":%lld,\"executed_jobs\":%u}\n",Baseline?"baseline":"current",name,ns,nt,sizeof(Input),(unsigned long long)large,(unsigned long long)largeBytes,(unsigned long long)count,(unsigned long long)bytes,(unsigned long long)copies,times[2],executed.load());
}
static void runtimePublication(){
 for(bool baseline:{true,false}){++scenarios;unloadFrames.clear();baselineUnloadFrames.clear();shared->unloadTask=0;shared->unloadInputSeq=0;externalInputFrame=true;fill(unloadExternalInput,1,1);
  for(int n=0;n<16;++n){if(baseline)publishUnloadCalculationBaseline();else publishUnloadCalculation();}
  watch(&unloadExternalInput,sizeof unloadExternalInput);measuring=true;
  for(int n=0;n<500;++n){if(baseline)publishUnloadCalculationBaseline();else publishUnloadCalculation();}
  measuring=false;check(!largeAllocations,"warmed history slots unexpectedly allocate");check(copiedBytes==1000*(baseline?sizeof unloadExternalInput:payload(unloadExternalInput)),"Runtime publication/history copy bytes differ");
  check(shared->unloadTask==516&&shared->unloadInputSeq==1032&&unloadCalculationPending,"Runtime serial or seqlock publication differs");same(shared->unloadInput,unloadExternalInput);
  for(unsigned serial=501;serial<=516;++serial){const auto*saved=baseline?baselineUnloadFrames.find(serial):unloadFrames.find(serial);check(saved!=nullptr,"one of sixteen history sources was lost");same(*saved,unloadExternalInput);}
  check(!(baseline?baselineUnloadFrames.find(500):unloadFrames.find(500)),"expired seventeenth history source remains visible");
  std::printf("RUNTIME {\"variant\":\"%s\",\"type\":\"unload\",\"calls\":500,\"copied_bytes\":%llu,\"input_allocations\":%llu,\"history_sources\":16}\n",baseline?"baseline":"current",(unsigned long long)copiedBytes,(unsigned long long)largeAllocations);
 }
 ++scenarios;const auto task=shared->unloadTask;externalInputFrame=false;publishUnloadCalculation();check(shared->unloadTask==task,"no-frame gate changed");externalInputFrame=true;unloadExternalInput.count=0;publishUnloadCalculation();check(shared->unloadTask==task,"empty-input gate changed");
}
static void rejectedCapture(){
 using Input=missilecompute::Input;using Output=missilecompute::Result;
 for(int reason=0;reason<9;++reason){++scenarios;compute::ValueTask<Input,Output>worker([](Input&i){return calculate(i);});auto source=std::make_unique<Input>();auto out=std::make_unique<Output>();fill(*source,1,1);snap.match=9;snap.frame=10;shared->computeFrame=10;shared->computeFrameMatch=9;shared->computeFrameSeq=2;volatile uint32_t request=1,computed=0;volatile long seq=2;barriers=0;
  if(reason==0)shared->computeFrameSeq=3;if(reason==1)seq=3;if(reason==2)request=0;if(reason==3)computed=request;
  if(reason==4)source->match=8;if(reason==5)source->frame=9;
  if(reason>=6)barrierHook=[&](unsigned n){if(n==2){if(reason==6)shared->computeFrameSeq+=2;else if(reason==7)++request;else seq+=2;}};
  service<false>(worker,request,computed,*source,*out,&seq);barrierHook={};
  check(worker.needs(planningGeneration,request,seq),"torn or mismatched capture submitted work");
  auto recovered=worker.acquire();check(bool(recovered),"rejected capture failed to return a reusable lease");
 }
}
static void ownership(){
 struct Input{unsigned value=0;};struct Output{unsigned value=0;};using Task=compute::ValueTask<Input,Output>;
 ++scenarios;typename Task::InputLease retained;
 {Task task([](Input&i){return Output{i.value};});std::vector<Task::InputLease>pins;std::set<Input*>addresses;
  for(unsigned n=0;n<8;++n){auto p=task.acquire();p->value=n;check(addresses.insert(p.get()).second,"pinned leases alias");pins.push_back(std::move(p));}
  for(unsigned n=0;n<8;++n)check(pins[n]->value==n,"fallback allocation overwrote a pinned lease");
  auto returned=pins.back().get();pins.pop_back();auto again=task.acquire();check(again.get()==returned,"free input storage was not reused");
  retained=std::move(pins[3]);retained->value=999;
 }
 check(retained&&retained->value==999,"lease did not survive ValueTask destruction");retained.reset();
 ++scenarios;{compute::ValueTask<compute::LoadInput,compute::LoadResult>task([](auto&i){return calculate(i);});inputSize=sizeof(compute::LoadInput);failInputAllocation=true;bool failed=false;try{auto input=task.acquire();}catch(const std::bad_alloc&){failed=true;}check(failed&&!failInputAllocation,"input allocation failure was not exercised");auto input=task.acquire();check(bool(input),"pool cannot recover after allocation failure");}
 ++scenarios;std::mutex mutex;std::condition_variable cv;bool entered=false,release=false;std::vector<unsigned>executed;
 Task task([&](Input&i){std::unique_lock<std::mutex>lock(mutex);executed.push_back(i.value);if(i.value==1){entered=true;cv.notify_one();cv.wait(lock,[&]{return release;});check(i.value==1,"in-flight mutable input was reused during invalidation");}return Output{i.value};});
 auto first=task.acquire();first->value=1;task.submit(1,1,2,std::move(first));{std::unique_lock<std::mutex>lock(mutex);check(cv.wait_for(lock,std::chrono::seconds(5),[&]{return entered;}),"ownership worker failed to start");}
 for(unsigned n=2;n<100;++n){auto pending=task.acquire();pending->value=n;task.submit(1,n,2,std::move(pending));}
 task.invalidate();auto fresh=task.acquire();fresh->value=100;task.submit(2,100,4,std::move(fresh));
 {std::lock_guard<std::mutex>lock(mutex);release=true;cv.notify_one();}
 const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(5);std::shared_ptr<const Task::Result>result;
 do{result=task.get();if(result)break;std::this_thread::yield();}while(std::chrono::steady_clock::now()<limit);
 check(result&&result->generation==2&&result->task==100&&result->sequence==4&&result->value.value==100,"invalidated result escaped or latest input failed");
 {std::lock_guard<std::mutex>lock(mutex);check(executed==std::vector<unsigned>({1,100}),"single-running/newest-pending ownership changed");}
 ++scenarios;bool closeEntered=false,closeRelease=false;std::atomic<bool>closing{false},closed{false};
 auto closeTask=std::make_unique<Task>([&](Input&i){std::unique_lock<std::mutex>lock(mutex);closeEntered=true;cv.notify_one();cv.wait(lock,[&]{return closeRelease;});return Output{i.value};});
 auto afterClose=closeTask->acquire();afterClose->value=777;
 auto executing=closeTask->acquire();executing->value=1;closeTask->submit(1,1,2,std::move(executing));
 {std::unique_lock<std::mutex>lock(mutex);check(cv.wait_for(lock,std::chrono::seconds(5),[&]{return closeEntered;}),"closing worker failed to start");}
 auto pending=closeTask->acquire();pending->value=2;closeTask->submit(1,2,2,std::move(pending));
 std::thread closer([owned=std::move(closeTask),&closing,&closed]()mutable{closing=true;owned.reset();closed=true;});
 while(!closing)std::this_thread::yield();check(!closed,"task destruction did not join its active worker");
 {std::lock_guard<std::mutex>lock(mutex);closeRelease=true;cv.notify_one();}closer.join();
 check(closed&&afterClose->value==777,"close invalidated an externally retained lease");afterClose.reset();
}
int main(){try{
 shared=sh=new Shared{};
 transferCases<unloadcompute::Input>("unload",unloadcompute::MaxSources,unloadcompute::MaxTargets);
 transferCases<missilecompute::Input>("missile",missilecompute::MaxSources,missilecompute::MaxTargets);
 transferCases<compute::LoadInput>("load",loading::MaxInfantry,loading::MaxVehicles);
 ownership();rejectedCapture();runtimePublication();
 hostMeasurement<true,unloadcompute::Input,unloadcompute::Result>("unload",1,1);hostMeasurement<false,unloadcompute::Input,unloadcompute::Result>("unload",1,1);
 hostMeasurement<true,missilecompute::Input,missilecompute::Result>("missile",1,1);hostMeasurement<false,missilecompute::Input,missilecompute::Result>("missile",1,1);
 hostMeasurement<true,compute::LoadInput,compute::LoadResult>("load",1,1);hostMeasurement<false,compute::LoadInput,compute::LoadResult>("load",1,1);
 hostMeasurement<true,unloadcompute::Input,unloadcompute::Result>("unload",32,64);hostMeasurement<false,unloadcompute::Input,unloadcompute::Result>("unload",32,64);
 hostMeasurement<true,missilecompute::Input,missilecompute::Result>("missile",32,64);hostMeasurement<false,missilecompute::Input,missilecompute::Result>("missile",32,64);
 hostMeasurement<true,compute::LoadInput,compute::LoadResult>("load",32,16);hostMeasurement<false,compute::LoadInput,compute::LoadResult>("load",32,16);
 hostMeasurement<true,unloadcompute::Input,unloadcompute::Result>("unload",256,16384);hostMeasurement<false,unloadcompute::Input,unloadcompute::Result>("unload",256,16384);
 hostMeasurement<true,missilecompute::Input,missilecompute::Result>("missile",256,256);hostMeasurement<false,missilecompute::Input,missilecompute::Result>("missile",256,256);
 hostMeasurement<true,compute::LoadInput,compute::LoadResult>("load",1024,256);hostMeasurement<false,compute::LoadInput,compute::LoadResult>("load",1024,256);
 delete shared;std::printf("SUMMARY {\"scenarios\":%u,\"checks\":%u,\"failures\":0}\n",scenarios,checks);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
