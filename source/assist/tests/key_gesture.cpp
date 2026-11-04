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
 // New modes have no layout cost and cannot collide with HOTKEYF_EXT.
 constexpr uint16_t press=uint16_t(Mode::Press),release=uint16_t(Mode::Release),hold=uint16_t(Mode::Hold),ctrl=0x200;
 assert(normalize('X'|press)==normalize('X'));
 assert(normalize('X'|hold,Mode::Hold)==normalize('X',Mode::Hold));
 assert(normalize('X'|release)!=normalize('X'|press));assert(normalize('X'|0x0800|release)==normalize('X'|release));
 memset(keys,0,256);keys[17]=1;assert(matchesHold(keys,17|hold));assert(!matchesHold(keys,17|press));assert(!matchesHold(keys,17|release));
 keys['X']=1;assert(matchesHold(keys,'X'|ctrl|hold));keys[17]=0;assert(!matchesHold(keys,'X'|ctrl|hold));
 auto description=describe('X'|ctrl|release,L"X");assert(description.find(L"按住 Ctrl")!=std::wstring::npos&&description.find(L"松开时触发")!=std::wstring::npos);
 description=describe('X'|Double|hold,L"X",true);assert(description.find(L"第二次保持按住")!=std::wstring::npos);
 t.clear();memset(keys,0,256);keys[17]=keys['X']=1;t.sample(keys,10);keys['X']=0;t.sample(keys,20);keys['X']=1;t.sample(keys,30);assert(t.qualifies('X'|ctrl|Double|hold));
 keys[17]=0;t.sample(keys,40);keys[17]=1;t.sample(keys,50);assert(!t.qualifies('X'|ctrl|Double|hold));

 std::vector<int> fired;auto emit=[&](const Group&group){fired.insert(fired.end(),group.actions.begin(),group.actions.end());};
 auto sample=[&](uint32_t now){r.sample(keys,now,1,emit);};
 auto setup=[&](uint16_t binding){r.clear();memset(keys,0,256);fired.clear();assert(r.add(7,binding,false,install));r.synchronize(keys,1);};
 setup('X'|ctrl|release);keys[17]=keys['X']=1;assert(!r.press(7,10,1,out));sample(11);assert(fired.empty()); // live key still down, no stale snapshot release
 keys['X']=0;sample(20);sample(21);assert(fired==std::vector<int>{7});
 fired.clear();keys['X']=1;assert(!r.press(7,30,1,out));keys[17]=0;sample(31);keys[17]=1;sample(32);keys['X']=0;sample(33);assert(fired.empty());
 keys['X']=1;assert(!r.press(7,40,1,out));keys[17]=keys['X']=0;sample(41);assert(fired.empty()); // ambiguous simultaneous prefix/main release is rejected
 keys[17]=keys['X']=1;assert(!r.press(7,50,1,out));keys['X']=0;sample(51);assert(fired==std::vector<int>{7});

 setup('A'|Double|release);keys['A']=1;assert(!r.press(7,10,1,out));sample(11);keys['A']=0;sample(20);assert(fired.empty());
 keys['A']=1;assert(!r.press(7,100,1,out));sample(101);assert(fired.empty());keys['A']=0;sample(110);sample(111);assert(fired==std::vector<int>{7});assert(!r.expired(500,1,out));
 setup('A'|release);assert(r.add(8,'A'|release|Double,false,install));keys['A']=1;assert(!r.press(7,10,1,out));keys['A']=0;sample(20);assert(fired.empty());
 assert(!r.expired(360,1,out));assert(r.expired(361,1,out)&&out.actions==std::vector<int>{7});
 // Existing Ctrl+X double shortcuts may release Ctrl between complete taps.
 setup('X'|ctrl|Double);keys[17]=keys['X']=1;assert(!r.press(7,10,1,out));keys['X']=0;sample(20);keys[17]=0;sample(30);keys[17]=keys['X']=1;assert(r.press(7,100,1,out)&&out.actions==std::vector<int>{7});
 setup('X'|ctrl|release|Double);keys[17]=keys['X']=1;assert(!r.press(7,10,1,out));keys[17]=keys['X']=0;sample(20);keys[17]=keys['X']=1;assert(!r.press(7,100,1,out));keys['X']=0;sample(110);assert(fired.empty()); // first release was not authorized

 setup(5|release);keys[5]=1;sample(10);assert(fired.empty());keys[5]=0;sample(20);assert(fired==std::vector<int>{7});
 setup(17|press);keys[17]=1;sample(10);sample(11);assert(fired==std::vector<int>{7});
 r.cancel();fired.clear();sample(20);keys[17]=0;sample(21);assert(fired.empty());keys[17]=1;sample(22);assert(fired==std::vector<int>{7});
 setup(4|press);keys[4]=1;r.synchronize(keys,1);sample(10);assert(fired.empty());keys[4]=0;sample(20);keys[4]=1;sample(30);assert(fired==std::vector<int>{7});
 assert(!r.add(8,'Q'|hold,false,install));assert(!r.add(9,4,false,install)); // explicit/legacy press have one identity
 Group merged;merged.actions={1,2,3};merged.bindings={'A',uint16_t('A'|press),uint16_t('A'|release)};firstPerGesture(merged);assert(merged.actions==std::vector<int>({1,3}));
 std::cout<<"PASS legacy double taps, mode/EXT normalization, press/release/double-release, live release sampling, prefix cancellation, simultaneous-release rejection, mouse/bare-modifier input, resume/rebind suppression and hold descriptions\n";
}
