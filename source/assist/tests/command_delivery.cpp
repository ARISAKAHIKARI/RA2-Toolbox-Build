#include "../source/command_delivery.h"
#include <cassert>
using namespace assist::delivery;
using assist::SelectType;
using assist::RevealMap;
int main(){
 // A reordered catalogue is a different intent, even when its slot is identical.
 Queue catalog;Item c{assist::CancelTechProduction,1,0,123,8,100,0,0,0,111};
 assert(catalog.add(c));c.typeHash=222;assert(catalog.add(c)&&catalog.size==2);
 Item pending;assert(catalog.peek(101,123,8,true,pending)&&pending.typeHash==111);
 assist::Type types[2]{};types[0].kind=16;types[1].kind=40;types[0].id[0]='A';types[1].id[0]='B';
 auto hash=assist::typeListHash(types,2);auto tmp=types[0];types[0]=types[1];types[1]=tmp;
 assert(hash!=assist::typeListHash(types,2));
 Queue q;Item a{9,0,0,123,UnknownMatch,100,0};
 assert(!q.add(Item{SelectType,1,0,123,UnknownMatch,100}));
 assert(!q.add(Item{assist::AutoBoard,0,0,123,UnknownMatch,100}));
 assert(q.add(a));assert(q.add(a));assert(q.size==1); // repeated key cannot flood queue
 Queue production;assert(production.add(Item{assist::Produce,1,0,123,8,200}));assert(production.add(Item{assist::Produce,1,0,123,8,201}));assert(production.size==2); // two explicit production clicks are two units
 Item out{};assert(!q.peek(150,123,1,false,out));
 assert(q.peek(150,123,8,true,out)&&out.match==8);q.consume();assert(q.size==0);
 // Rejoin with same PID must discard commands for old match.
 assert(q.add(Item{15,0,0,123,8,200,0}));assert(!q.peek(300,123,9,true,out));assert(q.size==0);
 // PID change and wraparound-safe expiry.
 assert(q.add(Item{9,0,0,123,0,200,0}));q.expire(201,777,0,false);assert(q.size==0);
 assert(q.add(Item{9,0,0,123,0,0xFFFFFFF0u,0}));q.expire(0x11u,123,0,false);assert(q.size==1);
 q.expire(0xFFFFFFF0u+MaxAgeMs+1u,123,0,false);assert(q.size==0);
 assert(focusIsRelevant(false,true));assert(focusIsRelevant(true,false));assert(!focusIsRelevant(false,false));
 assert(bridgeReady(1,true,false,true));assert(!bridgeReady(0,true,false,true));assert(!bridgeReady(1,true,true,true));assert(!bridgeReady(1,false,false,true));assert(!bridgeReady(1,true,false,false));
 // A game map still initializing may defer reveal for 300ms.
 assert(q.add(Item{9,0,0,123,4,10,1,320}));
 assert(!q.peek(100,123,4,true,out));assert(q.peek(320,123,4,true,out));q.consume();
 // Multiple commands preserve FIFO (do not reorder game orders).
 assert(q.add(Item{3,0,0,123,4,10,0}));assert(q.add(Item{9,0,0,123,4,11,0}));
 assert(q.peek(20,123,4,true,out)&&out.command==3);q.consume();assert(q.peek(20,123,4,true,out)&&out.command==9);
 // A -7 ACK is retried once, ONLY for this same match, with fresh intent.
 assert(retrySelection(-7,0,4100,100,9,9,true,false));
 assert(!retrySelection(-7,1,4100,100,9,9,true,false));
 assert(!retrySelection(-7,0,6100,100,9,9,true,false));
 assert(!retrySelection(-7,0,4100,100,9,10,true,false));
 assert(!retrySelection(-7,0,4100,100,9,9,false,false));
 assert(!retrySelection(-7,0,4100,100,9,9,true,true));
 // Selection is prioritized over slower queued actions, and the latest tap wins.
 q.clear();assert(q.add(Item{RevealMap,0,0,123,7,100,0}));
 assert(q.add(Item{SelectType,3,0,123,7,101,0}));
 assert(q.peek(102,123,7,true,out)&&out.command==SelectType&&out.arg==3&&!out.arg2);
 assert(q.add(Item{SelectType,3,1,123,7,150,0})); // double-tap supersedes the single tap
 assert(q.size==2);
 assert(q.peek(152,123,7,true,out)&&out.command==SelectType&&out.arg2==1);
 q.consume();assert(q.peek(152,123,7,true,out)&&out.command==RevealMap);
 // Switching from infantry A to tank B should not leave an old selection behind.
 q.clear();assert(q.add(Item{SelectType,3,0,123,7,200,0}));
 assert(q.add(Item{SelectType,4,0,123,7,250,0}));
 assert(q.size==1&&q.peek(255,123,7,true,out)&&out.arg==4);
 assert(!q.peek(2751,123,7,true,out)); // selection expires after 2.5 seconds
 assert(q.size==0);
 assert(q.add(Item{SelectType,1,1,123,7,3000,0}));
 assert(!q.peek(3010,123,8,true,out)); // never carry a selection into next match
 assert(q.size==0);

 // Only an explicit no-execution receipt may park a request. Keep its original
 // deadline and preserve its own lane while later unrelated input proceeds.
 Item unload{assist::SmartUnload,0,0,123,7,4000};
 assert(!q.park(unload,4001,0));
 assert(q.park(unload,4001,27));
 assert(q.add(unload));
 assert(q.add(Item{assist::AutoBoard,0,0,123,7,4002}));
 assert(q.add(Item{assist::ProduceDeferred,5,0,123,7,4003,0,0,11,995}));
 int index=-1;
 assert(q.peek(4004,123,7,true,out,&index)&&out.command==assist::AutoBoard&&index==2);
 q.discard(index);
 assert(q.peek(4005,123,7,true,out,&index)&&out.command==assist::ProduceDeferred&&index==2);
 q.discard(index);
 assert(!q.peek(4006,123,7,true,out));
 assert(q.peek(4081,123,7,true,out,&index)&&index==0&&out.created==4000&&out.token==27&&out.awaitingSample);
 q.discard(index);
 assert(!q.park(out,4082,28)&&q.park(out,4082,27));
 assert(!q.peek(6501,123,7,true,out)&&q.size==0);
 assert(!q.park(Item{assist::AttackOwnGround,0,0,123,7,7000},7001,0));
 assert(q.park(Item{assist::ProduceReserved,3,42,123,7,7000,0,0,71,996},7001,0));
 assert(!q.peek(7081,123,8,true,out)&&q.size==0);

 using namespace assist::commandsampling;
 SelectionReceipt receipt;
 SelectionMember live[]={{0x40000,31,4},{0x50000,32,0}};
 const auto first=receipt.capture(live,2);
 assert(first&&receipt.matches(live,2)&&receipt.capture(live,2)==first);
 ++live[1].manual;assert(!receipt.matches(live,2));
 assert(receipt.capture(live,2)!=first);
 live[1].pointer+=32;assert(!receipt.matches(live,2));
 receipt.clear();assert(!receipt.matches(live,2));
 assert(canAttempt(assist::Repair,Sample::Missing)&&canAttempt(assist::AutoBoard,Sample::Missing));
 assert(canAttempt(assist::ProduceDeferred,Sample::Missing)&&canAttempt(assist::ClearBeacons,Sample::Missing));
 assert(canAttempt(assist::SmartUnload,Sample::LocalCandidate)&&!canAttempt(assist::SmartUnload,Sample::Missing));
 assert(canAttempt(assist::ProduceReserved,Sample::LocalCandidate)&&!canAttempt(assist::ProduceReserved,Sample::Missing));
 assert(!canAttempt(SelectType,Sample::LocalCandidate)&&canAttempt(SelectType,Sample::Exact));
}
