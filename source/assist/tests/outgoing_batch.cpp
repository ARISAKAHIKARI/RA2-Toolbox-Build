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
 std::cout<<"PASS: outgoing batch backpressure / masks / restart / 24-slot bound\n";
}
