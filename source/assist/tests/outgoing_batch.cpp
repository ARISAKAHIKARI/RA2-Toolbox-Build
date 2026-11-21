#include "../source/outgoing_batch.h"
#include <cassert>
#include <iostream>
using assist::outgoing::Batch;
int main(){
 Batch s;
 s.begin(1);assert(s.request==1 && !s.done(5));
 s.accept();s.reject();
 assert(s.next==2 && s.accepted==1 && s.acceptedMask==1 && s.rejectedMask==2);
 // Queue-full across frames: no ack occurs, no entries are skipped.
 assert(!s.done(5));
 s.accept();s.accept();s.reject();
 assert(s.done(5) && s.next==5 && s.accepted==3);
 assert(s.acceptedMask==(1u|4u|8u));
 assert(s.rejectedMask==(2u|16u));
 // New request drops prior masks/cursor. Supports slot 23 in 24-item batches.
 s.begin(2);for(int i=0;i<24;i++) s.accept();
 assert(s.done(24) && s.acceptedMask==0xffffffu && s.rejectedMask==0);
 s.begin(3);assert(s.next==0 && !s.acceptedMask && !s.rejectedMask && !s.accepted);
 // Several 800-FPS frames may elapse between two exact samples. An unknown
 // item or an order requiring the full roster must not block proven attacks.
 auto eligible=[](int i){return i!=0;};
 int calls=0;
 auto admit=[&](int i){++calls;return i==1?0:i==2?-1:1;};
 assert(!s.candidatePass(7,4,800000,10,eligible,admit));
 assert(calls==4&&s.next==0&&s.acceptedMask==24&&s.rejectedMask==4);
 assert(!s.candidatePass(7,4,803999,10,eligible,admit));
 assert(calls==4); // A quick wake cannot bypass the work budget.
 assert(s.candidatePass(7,4,804000,10,eligible,admit));
 assert(calls==6&&s.acceptedMask==120&&!s.done(7));
 assert(s.candidatePass(7,4,810000,10,eligible,admit)&&calls==6);
 assert(s.candidatePass(7,4,811000,11,eligible,admit)&&calls==7);
 s.accept();assert(s.next==1);s.reject();
 assert(s.done(7)&&s.accepted==5&&s.rejectedMask==6);
 s.begin(4);for(int i=0;i<32;++i)s.accept();
 assert(s.done(32)&&s.acceptedMask==UINT32_MAX);
 std::cout<<"PASS: outgoing batch caps work across missed frames, keeps unknown orders and resumes exactly once\n";
}
