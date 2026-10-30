#define main roundTransitionRegression
#include "round_transition.cpp"
#undef main
#include "../source/presentation_state.h"
#include "../../shared/source/latest_worker.h"
#include <condition_variable>

struct Request {int generation=0;};
struct Result {int generation=0;std::shared_ptr<const um::Snapshot> snapshot;};
int main(){
 Fixture f;std::mutex gate;std::condition_variable cv;bool entered=false,released=false;
 auto ui=std::this_thread::get_id();
 f.m.duringSample=[&]{std::unique_lock<std::mutex> lock(gate);if(!entered){entered=true;cv.notify_all();cv.wait(lock,[&]{return released;});}};
 asyncwork::Latest<Request,Result> worker([&](const Request&r){
  assert(std::this_thread::get_id()!=ui);f.m.bookends=0;
  auto data=std::make_shared<const um::Snapshot>(f.sampler->sample(500,true));
  return std::make_shared<const Result>(Result{r.generation,data});
 });
 worker.submit({1});{std::unique_lock<std::mutex> lock(gate);assert(cv.wait_for(lock,std::chrono::seconds(2),[&]{return entered;}));}
 for(int i=0;i<100;++i)worker.submit({1});worker.invalidate();worker.submit({2});assert(!worker.get());
 {std::lock_guard<std::mutex> lock(gate);released=true;cv.notify_all();}
 std::shared_ptr<const Result> result;auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
 do{result=worker.get();if(!result)std::this_thread::yield();}while(!result&&std::chrono::steady_clock::now()<deadline);
 assert(result&&result->generation==2&&result->snapshot->status==1&&result->snapshot->playerCount==2);
 auto primary=result->snapshot,secondary=result->snapshot;assert(primary.get()==secondary.get());
 auto next=std::make_unique<um::Snapshot>(*primary);next->tick++;next->types[0].iconRetry++;next->types[0].iconCached^=1;
 assert(um::samePresentation(*primary,*next));next->players[0].cells[0].owned++;assert(!um::samePresentation(*primary,*next));
 next->players[0].cells[0].owned--;next->types[0].iconReady=1;next->types[0].icon[0]=10;assert(!um::samePresentation(*primary,*next));
 worker.shutdown();assert(primary->players[0].cells[0].owned==5);
 // The pointer index preserves the original first-match result for duplicates.
 auto indexSnapshot=std::make_unique<um::Snapshot>();indexSnapshot->typeCount=3;indexSnapshot->types[0].ptr=9;indexSnapshot->types[1].ptr=8;indexSnapshot->types[2].ptr=9;
 um::TypeIndex index;index.rebuild(*indexSnapshot);assert(index.find(9)==0&&index.find(8)==1&&index.find(7)==-1);index.clear();assert(index.find(9)==-1);
 std::cout<<"PASS off-thread sampler, invalidation while blocked, immutable shared snapshots, visual change filtering and duplicate type lookup\n";
}
