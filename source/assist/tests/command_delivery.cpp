#include "../source/command_delivery.h"
#include <cassert>
using namespace assist::delivery;
using assist::SelectType;
using assist::RevealMap;
int main(){
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
}
