#pragma once
#include "external_planner.h"
#include <chrono>
#include <memory>
#include <tuple>

namespace assist { namespace planner {
struct Parameters {
 uint32_t flags=0,modes=3;int veteran=200,elite=300,timeout=90,hz=10,priority=0,range=2;bool drop=false,observeDrop=false;
 bool operator==(const Parameters&o)const{return std::tie(flags,modes,veteran,elite,timeout,hz,priority,range,drop,observeDrop)==std::tie(o.flags,o.modes,o.veteran,o.elite,o.timeout,o.hz,o.priority,o.range,o.drop,o.observeDrop);}
};
struct Receipt {Order order{};int frame=0;enum Kind {Accepted,Rejected,Expired} kind=Expired;};
struct Statistics {
 uint32_t oreBypasses=0,oreYieldAttempts=0,oreTurnTimeouts=0,oreDelivered=0,oreUnverified=0,magnetShots=0,magnetUnverified=0,tracking=0,targetChecks=0;
 int queued=0,docking=0,issued=0,yielded=0;std::vector<uint32_t> stopped;
};
struct TaskInput {
 uint64_t generation=0,serial=0;std::shared_ptr<const World> world;Parameters parameters;std::vector<Receipt> receipts;
};
struct TaskOutput {
 uint64_t generation=0,serial=0;uint32_t match=0,tick=0,micros=0;int frame=-1;Parameters parameters;
 Statistics statistics;std::vector<Order> orders;
};
// Only this persistent worker owns Planner's mutable jobs and indices. The UI
// transfers immutable observations and ordered receipts, never a Shared/game
// pointer. Retain the World while the indices contain addresses into it.
class Task {
 Planner planner_;uint64_t generation_=0;std::shared_ptr<const World> world_;
public:
 std::shared_ptr<const TaskOutput> run(const TaskInput&i){
  if(!i.world)return {};
  if(generation_!=i.generation){planner_.reset();world_.reset();generation_=i.generation;}
  for(const auto&r:i.receipts){if(r.kind==Receipt::Accepted)planner_.accepted(r.order,r.frame);else if(r.kind==Receipt::Rejected)planner_.rejected(r.order,r.frame);else planner_.expired(r.order);}
  world_=i.world;auto out=std::make_shared<TaskOutput>();out->generation=i.generation;out->serial=i.serial;out->match=world_->match;out->tick=world_->tick;out->frame=world_->frame;out->parameters=i.parameters;
  const auto begin=std::chrono::steady_clock::now();const auto&p=i.parameters;
  out->orders=planner_.step(*world_,p.flags,p.veteran,p.elite,p.timeout,p.hz,p.priority,p.range,p.drop,p.modes,p.observeDrop);
  out->micros=uint32_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count());
  auto&s=out->statistics;s.oreBypasses=planner_.oreBypasses;s.oreYieldAttempts=planner_.oreYieldAttempts;s.oreTurnTimeouts=planner_.oreTurnTimeouts;
  s.oreDelivered=planner_.oreDelivered;s.oreUnverified=planner_.oreUnverified;s.magnetShots=planner_.magnetShots;s.magnetUnverified=planner_.magnetUnverified;
  s.queued=planner_.queued;s.docking=planner_.docking;s.issued=planner_.issued;s.yielded=planner_.yielded;s.tracking=planner_.trackingFlags();s.targetChecks=planner_.targetChecks;s.stopped=planner_.stoppedUnits();return out;
 }
};
}}
