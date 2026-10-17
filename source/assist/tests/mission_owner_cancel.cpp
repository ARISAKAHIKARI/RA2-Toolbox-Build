#include "../source/command_gateway.h"
#include <cassert>
using namespace assist::gateway;
using namespace assist::taskarbiter;
int main(){
 MissionObserver<4> mo;int called=0;
 auto s=mo.record(11,8,9,7,500,100,{true,0,-1,0},Owner::AutoLoad);
 assert(s);bool found=mo.cancelTicket(11,s,Failure::Superseded,[&](const Receipt&r){
  assert(r.serial==s&&r.owner==Owner::AutoLoad&&r.phase==Phase::Cancelled);
  assert(r.failure==Failure::Superseded);++called;
 });
 assert(found&&called==1&&!mo.pending());
 assert(!mo.cancelTicket(11,s,Failure::Superseded,[&](const Receipt&){assert(false);}));
 auto p=mo.record(12,8,9,7,100,200,{true,0,-1,0},Owner::Cooperative);
 auto q=mo.record(12,9,9,7,100,200,{true,0,-1,0},Owner::Magnetron);
 assert(p&&q&&p!=q);
 assert(mo.cancelTicket(12,p,Failure::Superseded,[&](const Receipt&r){assert(r.owner==Owner::Cooperative);}));
 int observed=0;
 mo.pollWithReceipts(12,101,201,[](uint32_t){return MissionState{true,7,-1,0};},[&](const Receipt&r){
  assert(r.serial==q&&r.owner==Owner::Magnetron);++observed;
 });
 assert(observed==1&&!mo.pending());
 auto z=mo.record(15,20,5,7,100,300,{true,0,-1,0},Owner::AutoLoad);
 assert(mo.dismissTicket(15,z));assert(!mo.pending());
 assert(!mo.dismissTicket(15,z));
}
