#include "../../shared/source/latest_worker.h"
#include "../../shared/source/recurring_worker.h"
#include "../source/render_observation_probe.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <functional>
#include <future>
using namespace std::chrono_literals;
struct Retired {std::function<void()> callback;~Retired(){if(callback)callback();}};
struct Input {int value=0;std::shared_ptr<Retired> owned;};
struct Output {int value=0;std::shared_ptr<Retired> owned;};
template<class Predicate>void eventually(Predicate p){auto deadline=std::chrono::steady_clock::now()+3s;while(!p()){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::sleep_for(1ms);}}
template<template<class,class>class Worker>void retirement(){
 std::atomic<int> inputRetired{0},outputRetired{0},published{0};
 std::function<void()> query;
 Worker<Input,Output> worker([&](const Input&i){auto out=std::make_shared<Output>();out->value=i.value;out->owned=std::make_shared<Retired>();out->owned->callback=[&]{query();++outputRetired;};return out;},[&]{++published;});
 query=[&]{auto current=worker.get();};
 auto send=[&](int n){Input i;i.value=n;i.owned=std::make_shared<Retired>();i.owned->callback=[&]{query();++inputRetired;};if constexpr(std::is_same_v<Worker<Input,Output>,asyncwork::Recurring<Input,Output>>)worker.submit(std::move(i),5,500ms);else worker.submit(std::move(i));};
 send(1);eventually([&]{auto p=worker.get();return p&&p->value==1;});
 send(2);eventually([&]{auto p=worker.get();return p&&p->value==2;});
 eventually([&]{return inputRetired>=1&&outputRetired>=1;});
 worker.invalidate();eventually([&]{return inputRetired>=2&&outputRetired>=2;});assert(!worker.get());
 send(3);eventually([&]{auto p=worker.get();return p&&p->value==3;});
 worker.shutdown();assert(!worker.get());assert(inputRetired==3&&outputRetired>=3);
}
template<template<class,class>class Worker>void staleCompletion(){
 std::promise<void> started,release;auto gate=release.get_future().share();
 Worker<int,int> worker([&](const int&i){started.set_value();gate.wait();return std::make_shared<const int>(i);});
 if constexpr(std::is_same_v<Worker<int,int>,asyncwork::Recurring<int,int>>)worker.submit(7,60);else worker.submit(7);
 started.get_future().wait();worker.invalidate();release.set_value();worker.shutdown();assert(!worker.get());
}
void expiry(){
 std::atomic<int> retired{0};std::function<void()>query;
 asyncwork::Recurring<int,Output> worker([&](const int&i){auto p=std::make_shared<Output>();p->value=i;p->owned=std::make_shared<Retired>();p->owned->callback=[&]{query();++retired;};return p;});query=[&]{auto p=worker.get();};
 worker.submit(4,5,30ms);eventually([&]{return bool(worker.get());});eventually([&]{return retired>=1;});assert(!worker.get());worker.shutdown();
}
void probe(){
 int calls=0,frame=100;bool readable=true;
 auto read=[&](uint32_t at,void*out,size_t n){++calls;assert(at==0xA8ED84&&n==4);if(readable)*static_cast<int*>(out)=frame;return readable;};
 // Each unchanged high-frequency tick now costs one read instead of a whole
 // production mainview sample. The original fifty-millisecond refresh stays.
 for(int i=1;i<50;++i)assert(renderobservation::duplicate(read,100,1000.,1000.+i));assert(calls==49);
 assert(!renderobservation::duplicate(read,100,1000.,1050.));assert(calls==49);
 frame=101;assert(!renderobservation::duplicate(read,100,1000.,1001.));
 frame=1;assert(!renderobservation::duplicate(read,100,1000.,1001.));
 readable=false;assert(!renderobservation::duplicate(read,100,1000.,1001.));
 assert(!renderobservation::duplicate(read,-1,1000.,1001.));
 assert(!renderobservation::duplicate(read,100,1000.,999.));
 assert(!renderobservation::duplicate(read,100,1000.,NAN));
}
int main(){probe();retirement<asyncwork::Latest>();retirement<asyncwork::Recurring>();staleCompletion<asyncwork::Latest>();staleCompletion<asyncwork::Recurring>();expiry();puts("PASS production Latest/Recurring replacement, invalidate, shutdown, expiry and stale completion; reentrant destructor never owns scheduling lock; duplicate observer preserves fifty-ms refresh/failed-read/new-frame semantics");}
