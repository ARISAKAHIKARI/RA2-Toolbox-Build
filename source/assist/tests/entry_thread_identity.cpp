#include <cassert>
#include <cstdio>
#include "../../shared/source/entry_policy.h"
using namespace gameentry;
static Request request(Operation operation,bool bind=false){
 Request r{};r.operation=operation;r.client=Client::Assist;r.module=0x10000000;r.bindThread=bind;return r;
}
int main(){
 Policy p;auto bound=request(Operation::Enter,true);
 assert(p.enter(100,bound)==Result::Ok);assert(p.leave(100,bound)==Result::Ok);
 // A still-live identity may enter non-binding scopes on its bound thread.
 auto live=request(Operation::Capture);assert(p.enter(100,live)==Result::Ok);assert(p.leave(100,live)==Result::Ok);
 // The kernel can reuse the number after the retained thread object exits.
 // This fact must revoke a stale numeric binding, not authorize a callback.
 auto recycled=request(Operation::Capture);
 assert(p.enter(100,recycled,true)==Result::WrongThread);
 auto unbound=request(Operation::Enter);assert(p.enter(100,unbound,true)==Result::WrongThread);
 assert(!p.module&&!p.depth);
 auto rebound=request(Operation::Enter,true);
 assert(p.enter(100,rebound,true)==Result::Ok);assert(p.leave(100,rebound)==Result::Ok);
 // A new numeric thread likewise needs both death proof and explicit bind.
 auto other=request(Operation::Enter,true);assert(p.enter(200,other,false)==Result::WrongThread);
 assert(p.enter(200,other,true)==Result::Ok);assert(p.leave(200,other)==Result::Ok);
 puts("PASS entry identity: live thread accepted; recycled TID cannot capture; known-dead identity requires explicit fresh bind");
}
