#include "../../shared/source/parallel_rows.h"
#include <cassert>
#include <iostream>
#include <set>
int main(){
 std::vector<unsigned> output(1024*1024);std::mutex mutex;std::set<std::thread::id> ids;
 for(unsigned epoch=1;epoch<=80;++epoch){
  parallelrows::rows(1024,output.size(),[&](size_t first,size_t last){
   {std::lock_guard<std::mutex>lock(mutex);ids.insert(std::this_thread::get_id());}
   for(size_t row=first;row<last;++row)for(size_t x=0;x<1024;++x)output[row*1024+x]=unsigned(row*1024+x)^epoch;
   // A nested operation must run inline, never deadlock its own team.
   parallelrows::rows(last-first,output.size(),[](size_t,size_t){});
  });
  for(size_t i=0;i<output.size();++i)assert(output[i]==(unsigned(i)^epoch));
 }
 // Two renderer owners may request work concurrently. Busy callers keep
 // computing locally; neither can consume the other's captured references.
 std::atomic<unsigned> completed{0};auto other=[&]{for(unsigned n=0;n<100;++n){std::vector<unsigned> rows(512,0);
  parallelrows::rows(512,1024*1024,[&](size_t first,size_t last){for(size_t r=first;r<last;++r)++rows[r];});
  for(auto value:rows)assert(value==1);++completed;
 }};std::thread a(other),b(other);a.join();b.join();assert(completed==200);
 std::cout<<"PASS persistent row pool: 80 exact frames, nested calls, 200 concurrent submissions; observed_threads="<<ids.size()<<"\n";
}
