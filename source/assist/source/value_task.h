#pragma once
#include "../../shared/source/latest_worker.h"
#include <functional>
namespace assist { namespace compute {
// A lazy, persistent value-only lane. One executing job and one replaceable
// input bound memory. Publication and freshness checks remain on the UI.
template<class Input,class Output>class ValueTask {
public:
 struct Result {uint64_t generation=0;uint32_t task=0;long sequence=0;Output value;};
private:
 struct Request {uint64_t generation;uint32_t task;long sequence;std::shared_ptr<Input> input;};
 std::function<Output(Input&)> calculate_;asyncwork::Latest<Request,Result> worker_;
 uint64_t generation_=0;uint32_t task_=0;long sequence_=0;bool submitted_=false;uint32_t submittedAt_=0;
public:
 explicit ValueTask(std::function<Output(Input&)> calculate,std::function<void()> completed={}):calculate_(std::move(calculate)),worker_([this](const Request&r){auto out=std::make_shared<Result>();out->generation=r.generation;out->task=r.task;out->sequence=r.sequence;out->value=calculate_(*r.input);return out;},std::move(completed)){}
 bool ready(uint32_t now,uint32_t period)const{return !submitted_||uint32_t(now-submittedAt_)>=period;}
 void started(uint32_t now){submittedAt_=now;}
 bool needs(uint64_t generation,uint32_t task,long sequence=0)const{return !submitted_||generation!=generation_||task!=task_||sequence!=sequence_;}
 void submit(uint64_t generation,uint32_t task,long sequence,std::shared_ptr<Input> input){generation_=generation;task_=task;sequence_=sequence;submitted_=true;worker_.submit({generation,task,sequence,std::move(input)});}
 std::shared_ptr<const Result> get(){return worker_.get();}
 void invalidate(){worker_.invalidate();submitted_=false;}
};
}}
