#pragma once
#include "../../shared/source/latest_worker.h"
#include <array>
#include <functional>
namespace assist { namespace compute {
// A lazy, persistent value-only lane. One job executes and one pending input
// can be replaced; up to three idle inputs are cached. Pinned inputs are never
// reused. Publication and freshness checks remain on the UI.
template<class Input,class Output>class ValueTask {
 struct InputPool {
  std::mutex mutex;std::array<std::unique_ptr<Input>,3> free;
  Input*take(){
   {std::lock_guard<std::mutex>lock(mutex);for(auto&slot:free)if(slot)return slot.release();}
   return new Input{};
  }
  void give(Input*input){
   std::unique_ptr<Input>retired(input);
   {std::lock_guard<std::mutex>lock(mutex);for(auto&slot:free)if(!slot){slot.swap(retired);break;}}
  }
 };
 struct ReturnInput {
  std::shared_ptr<InputPool>pool;
  void operator()(Input*input)const{pool->give(input);}
 };
public:
 // A lease owns its storage exclusively. Returning from the worker hands it
 // back under the pool mutex, which also orders the mutable load calculation's
 // writes before reuse. A retained lease keeps its pool alive beyond this task.
 using InputLease=std::unique_ptr<Input,ReturnInput>;
 struct Result {uint64_t generation=0;uint32_t task=0;long sequence=0;Output value;};
private:
 struct Request {uint64_t generation;uint32_t task;long sequence;InputLease input;};
 std::function<Output(Input&)> calculate_;std::shared_ptr<InputPool>pool_=std::make_shared<InputPool>();
 // Destroy/join the worker before releasing the owning pool or calculate_.
 asyncwork::Latest<Request,Result> worker_;
 uint64_t generation_=0;uint32_t task_=0;long sequence_=0;bool submitted_=false;uint32_t submittedAt_=0;
public:
 explicit ValueTask(std::function<Output(Input&)> calculate,std::function<void()> completed={}):calculate_(std::move(calculate)),worker_([this](const Request&r){auto out=std::make_shared<Result>();out->generation=r.generation;out->task=r.task;out->sequence=r.sequence;out->value=calculate_(*r.input);return out;},std::move(completed)){}
 InputLease acquire(){return InputLease(pool_->take(),ReturnInput{pool_});}
 bool ready(uint32_t now,uint32_t period)const{return !submitted_||uint32_t(now-submittedAt_)>=period;}
 void started(uint32_t now){submittedAt_=now;}
 bool needs(uint64_t generation,uint32_t task,long sequence=0)const{return !submitted_||generation!=generation_||task!=task_||sequence!=sequence_;}
 void submit(uint64_t generation,uint32_t task,long sequence,InputLease input){generation_=generation;task_=task;sequence_=sequence;submitted_=true;worker_.submit({generation,task,sequence,std::move(input)});}
 std::shared_ptr<const Result> get(){return worker_.get();}
 void invalidate(){worker_.invalidate();submitted_=false;}
};
}}
