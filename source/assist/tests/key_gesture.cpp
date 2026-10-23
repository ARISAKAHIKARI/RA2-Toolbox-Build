#include "../../shared/source/key_gesture.h"
#include "../source/hotkey_routes.h"
#include "../source/pointer_intent.h"
#include <cassert>
#include <iostream>
int main(){using namespace keygesture;Tracker t;uint8_t keys[256]{};
 keys['A']=1;t.sample(keys,10);assert(!(t.held['A']&2));t.sample(keys,20);assert(!(t.held['A']&2));keys['A']=0;t.sample(keys,50);keys['A']=1;t.sample(keys,300);assert(t.held['A']==3);t.decorate(keys);assert(assist::pointerinput::matchesHold(keys,'A'|Double,assist::pointerinput::Enhanced));
 keys['A']=0;t.sample(keys,310);assert(!t.qualifies('A'|Double));keys['A']=1;t.sample(keys,350);assert(t.held['A']==1);keys['A']=0;t.sample(keys,360);keys['A']=1;t.sample(keys,800);assert(t.held['A']==1);
 t.clear();memset(keys,0,256);keys[17]=1;t.sample(keys,10);keys[17]=0;t.sample(keys,20);keys[17]=1;t.sample(keys,100);t.decorate(keys);assert(assist::pointerinput::matchesHold(keys,17|Double,assist::pointerinput::Enhanced));
 t.clear();memset(keys,0,256);keys[17]=keys[162]=1;t.sample(keys,10);keys[17]=keys[162]=0;t.sample(keys,20);keys[17]=keys[162]=1;t.sample(keys,100);assert(t.held[17]==3);
 t.clear();t.edge('A',true,false,0xfffffff0,'A');t.edge('A',false,false,5,0);t.edge('A',true,false,10,'A');assert(t.held['A']==3);
 auto beforeSerial=t.next;t.clear();t.edge('B',true,false,20,'B');assert(t.serial['B']>beforeSerial);
 t.clear();t.edge(17,true,false,10,17);t.edge('C',true,false,20,'C'|512);t.edge(17,false,false,30,0);t.edge(17,true,false,40,17);assert(t.held[17]==1);
 using namespace assist::hotkeyroute;Registry r;int installs=0;auto install=[&](int,uint16_t){++installs;return true;};assert(r.add(1,'A',false,install));assert(r.add(2,'A'|Double,false,install));assert(installs==1&&!r.add(3,'A'|Double,false,install));
 Group out;assert(!r.press(1,10,1,out));assert(!r.expired(360,1,out));assert(r.expired(361,1,out)&&out.actions==std::vector<int>{1});
 assert(!r.press(1,500,1,out));assert(r.press(1,700,1,out)&&out.actions==std::vector<int>{2});assert(!r.expired(1100,1,out));
 assert(!r.press(1,1200,1,out));assert(r.press(1,1600,1,out)&&out.actions==std::vector<int>{1});assert(r.press(1,1700,1,out)&&out.actions==std::vector<int>{2});assert(!r.press(1,1800,1,out));assert(!r.expired(2200,2,out));r.clear();assert(r.add(4,'B'|Double,true,install));assert(r.add(5,'B'|Double,true,install));assert(!r.press(4,1,3,out));assert(r.press(4,2,3,out)&&out.actions.size()==2);
 std::cout<<"PASS double tap release/repeat/deadline/wrap/chord cancellation/hold modifiers; single-double arbitration and shared binding fanout\n";
}
