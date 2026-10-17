#include "../source/command_gateway.h"
#include <cassert>
using namespace assist::gateway;
int main(){
 MissionObserver<4> t; int expired=0,cancelledCount=0,observedCount=0;
 auto report=[&](const Receipt &r){
  if(r.phase==Phase::Expired)++expired;
  if(r.phase==Phase::Cancelled)++cancelledCount;
  if(r.phase==Phase::Observed)++observedCount;
 };
 auto sample=[](uint32_t){return MissionState{true,5,5,0};};
 uint32_t a=t.record(11,555,0,2,5,100,{true,5,5,0});
 uint32_t b=t.record(11,556,0,2,5,100,{true,5,5,0});
 assert(a&&b&&a!=b);
 t.clear();
 t.record(11,100,0,2,5,100,{true,5,5,0});
 assert(t.pollWithReceipts(11,306,101,sample,report)==0);
 assert(expired==1&&cancelledCount==0&&!t.pending());
 t.record(11,101,0,2,306,102,{true,5,5,0});
 t.pollWithReceipts(12,307,103,sample,report);
 assert(cancelledCount==1&&!t.pending());
 t.record(12,102,0,2,308,104,{true,5,5,0});
 auto observe=[](uint32_t){return MissionState{true,2,5,0};};
 assert(t.pollWithReceipts(12,309,105,observe,report)==1);
 assert(observedCount==1&&!t.pending());
}
