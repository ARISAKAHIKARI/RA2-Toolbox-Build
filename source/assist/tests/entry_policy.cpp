#include "../../shared/source/entry_policy.h"
#include <cassert>
using namespace gameentry;
int main(){
 Policy p;Request a{};a.operation=Operation::Enter;a.module=0x10000000;a.client=Client::Assist;
 assert(p.enter(10,a)==Result::WrongThread);
 Request unbound=a;unbound.operation=Operation::Capture;unbound.bindThread=1;
 assert(p.enter(10,unbound)==Result::Invalid);
 a.bindThread=1;assert(p.enter(10,a)==Result::Ok);const auto outer=a;
 Request cursor=a;cursor.module=0x20000000;cursor.client=Client::Cursor;
 assert(p.enter(10,cursor)==Result::Busy);
 Request reentry=a;reentry.bindThread=0;assert(p.enter(10,reentry)==Result::Busy);
 Request child=a;child.operation=Operation::Capture;child.bindThread=0;
 assert(p.enter(10,child)==Result::Ok&&p.depth==2&&child.token!=outer.token);
 auto staleChild=child;auto earlyOuter=outer;
 assert(p.leave(10,earlyOuter)==Result::WrongOwner&&p.depth==2);
 assert(p.leave(10,child)==Result::Ok&&p.depth==1&&child.token==outer.token);
 assert(p.leave(10,staleChild)==Result::WrongOwner&&p.depth==1);
 assert(p.leave(11,a)==Result::WrongOwner);
 Request nested[Policy::MaxDepth]{};
 for(unsigned i=1;i<Policy::MaxDepth;++i){nested[i]=a;nested[i].operation=Operation::Capture;nested[i].bindThread=0;nested[i].token=p.token;assert(p.enter(10,nested[i])==Result::Ok);}
 auto overflow=nested[Policy::MaxDepth-1];assert(p.enter(10,overflow)==Result::Busy);
 for(unsigned i=Policy::MaxDepth-1;i>0;--i)assert(p.leave(10,nested[i])==Result::Ok);
 assert(p.leave(10,a)==Result::Ok&&p.module==0&&p.depth==0);
 a=outer;assert(p.enter(12,a,false)==Result::WrongThread);
 assert(p.enter(12,a,true)==Result::Ok&&p.thread==12);assert(p.leave(12,a)==Result::Ok);
 Policy provisional;cursor.token=0;assert(provisional.enter(20,cursor)==Result::Ok);assert(provisional.leave(20,cursor)==Result::Ok);
 a=outer;assert(provisional.enter(21,a,false)==Result::WrongThread);
 assert(provisional.enter(21,a,true)==Result::Ok);assert(provisional.leave(21,a)==Result::Ok);
 p.serial=0xffffffff;a=outer;assert(p.enter(12,a)==Result::Ok&&a.token==1);assert(p.leave(12,a)==Result::Ok);
}
