#include "../../shared/source/latest_worker.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>
#include <atomic>
int main(){
 std::mutex mutex;std::condition_variable cv;bool entered=false,released=false,second=false;std::vector<int> executed;
 std::atomic<unsigned> callbacks{0};asyncwork::Latest<int,int>*published=nullptr;
 const auto ui=std::this_thread::get_id();
 asyncwork::Latest<int,int> worker([&](const int&value){
  assert(std::this_thread::get_id()!=ui);std::unique_lock<std::mutex> lock(mutex);executed.push_back(value);
  if(value==1){entered=true;cv.notify_all();cv.wait(lock,[&]{return released;});}
  else{second=true;cv.notify_all();}return std::make_shared<const int>(value);
 },[&]{assert(published&&published->get());++callbacks;});published=&worker;
 worker.submit(1);{std::unique_lock<std::mutex> lock(mutex);assert(cv.wait_for(lock,std::chrono::seconds(2),[&]{return entered;}));}
 // The worker is blocked inside computation. Publishing inputs, reading the
 // last completed image and changing world must remain usable on this thread.
 for(int i=2;i<1000;++i)worker.submit(i);
 assert(!worker.get());worker.invalidate();worker.submit(1000);
 {std::lock_guard<std::mutex> lock(mutex);released=true;cv.notify_all();}
 {std::unique_lock<std::mutex> lock(mutex);assert(cv.wait_for(lock,std::chrono::seconds(2),[&]{return second;}));}
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);std::shared_ptr<const int> frame;
 do{frame=worker.get();if(frame)assert(*frame==1000);std::this_thread::yield();}while(!frame&&std::chrono::steady_clock::now()<deadline);
 assert(frame&&*frame==1000);worker.shutdown();assert(callbacks==1);assert((executed==std::vector<int>{1,1000}));
 // Completed immutable storage survives shutdown while a presenter owns it.
 assert(*frame==1000);std::cout<<"PASS nonblocking submission, independent execution, bounded newest-only queue, stale-world rejection and shutdown ownership\n";
}
