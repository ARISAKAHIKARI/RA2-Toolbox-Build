#include "../../shared/source/recurring_worker.h"
#include <atomic>
#include <cassert>
#include <cstdio>
using namespace std::chrono_literals;
template<class F>void eventually(F f){const auto end=std::chrono::steady_clock::now()+3s;while(!f()){assert(std::chrono::steady_clock::now()<end);std::this_thread::yield();}}
int main(){
 std::atomic<int> value{-1};
 asyncwork::Recurring<int,int> worker([&](const int&i){value=i;return std::make_shared<const int>(i);});
 const auto before=asyncwork::wakeProbeRequests.load();worker.submit(0,5,2s);
 eventually([&]{return value==0;});
 for(int i=1;i<=1000;++i)worker.submit(i,5,2s);
 const auto requests=asyncwork::wakeProbeRequests.load()-before;
#ifdef EXPECT_RENEWAL_FAST
 assert(requests==1);
#else
 assert(requests==1001);
#endif
 eventually([&]{return value==1000;});
 // A shorter lease must interrupt a pending 200 ms deadline and retire output.
 const auto shorter=asyncwork::wakeProbeRequests.load();worker.submit(1001,5,1ms);
 assert(asyncwork::wakeProbeRequests.load()==shorter+1);eventually([&]{return !worker.get();});
 worker.submit(1002,5,2s);eventually([&]{return value==1002;});
 const auto changed=asyncwork::wakeProbeRequests.load();worker.submit(1003,1000,2s);
 assert(asyncwork::wakeProbeRequests.load()==changed+1);eventually([&]{return value==1003;});
 worker.invalidate();assert(!worker.get());worker.submit(1004,5,2s);eventually([&]{return value==1004;});
#ifdef EXPECT_RENEWAL_FAST
 auto shared=std::make_shared<const int>(1005);
 worker.submitShared(shared,1000,2s);eventually([&]{return value==1005;});
 worker.submitShared({},1000,2s);assert(worker.get());
#endif
 worker.shutdown();
 std::printf("{\"case\":\"same_rate_renewals\",\"submissions\":1001,\"wake_requests\":%llu,\"passed\":true}\n",(unsigned long long)requests);
}
