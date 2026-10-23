#include "../source/hotkey_routes.h"
#include "../source/pointer_batch.h"
#include <cassert>
#include <iostream>
using namespace assist;
int main(){
 hotkeyroute::Registry registry;int registrations=0;auto install=[&](int,uint16_t k){++registrations;return k!=999;};
 assert(registry.add(10,0x641,false,install));assert(!registry.add(11,0xe41,false,install));assert(registrations==1);
 assert(registry.add(11,0xe41,true,install));assert(registry.add(12,0x641,true,install));assert(registry.add(11,0x641,true,install));
 assert(registry.find(10)->actions==std::vector<int>({10,11,12}));assert(!registry.find(11));assert(registrations==1);
 assert(!registry.add(13,999,true,install));assert(!registry.find(13));
 hotkeyroute::Pending queue;assert(queue.add(*registry.find(10),100,8));hotkeyroute::Action a;
 for(int id:{10,11,12}){assert(queue.pop(101,8,a));assert(a.id==id&&a.tick==100&&a.match==8);}assert(!queue.pop(101,8,a));
 assert(queue.add(*registry.find(10),0xfffffff0u,8));assert(queue.pop(0x10,8,a));queue.clear();
 assert(queue.add(*registry.find(10),1,8));assert(!queue.pop(2502,8,a));
 assert(queue.add(*registry.find(10),100,8));assert(!queue.pop(101,9,a));
 hotkeyroute::Group full{1,1,std::vector<int>(128,7)};assert(queue.add(full,100,8));assert(!queue.add(*registry.find(10),100,8));assert(queue.count==128);
 // A full blocked queue releases expired slots before the next insertion.
 assert(queue.add(*registry.find(10),2701,8));assert(queue.count==3);queue.expire(2702,9);assert(queue.count==0);
 queue.clear();registry.clear();assert(!registry.find(10));
 using namespace pointerinput;uint8_t keys[256]{};uint32_t buttons[MouseCount];for(auto&b:buttons)b=1;
 Bindings c{0,0,0,0,0,0,0,0,1023};MouseRoute routes[MouseCount+1]{};
 assert(resolveMouseMatches(keys,1,buttons,c,SellDrainedInput|SellVehicleInput,false,routes)==1);
 assert(routes[0].operation==Enhanced&&(routes[0].bindings&7)==7);
 assert(resolveMouseMatches(keys,1,buttons,c,SellDrainedInput|SellVehicleInput,true,routes)==10);
 const uint32_t expect[]={Advanced,Magnetron,CS,Enter,AirFirst,AirSecond,Control,0,Control|SellDrainedInput,Control|SellVehicleInput};
 for(int j=0;j<10;++j){assert(routes[j].operation==(j<7?Enhanced:j==7?AttackMove:Sell));assert(routes[j].bindings==(expect[j]|PerFeatureMouse));}
 c.advanced='G';keys['G']=1;assert(resolveMouseMatches(keys,1,buttons,c,0,true,routes)==8);assert((routes[0].bindings&Advanced)&&(routes[1].bindings&Magnetron));keys['G']=0;
 c.enabled=0;for(auto&b:buttons)b=0;assert(!resolveMouseMatches(keys,1,buttons,c,0,true,routes));
 assert(resolveMouseMatches(keys,4,buttons,c,0,true,routes)==1&&routes[0].operation==FollowNative);
 for(bool all:{false,true}){assert(resolveMouseMatches(keys,6,buttons,c,0,all,routes)==1);assert(routes[0].operation==FollowNative&&(routes[0].bindings&LeftClick));}
 assert(resolveMouseMatches(keys,5,buttons,c,0,true,routes)==1&&routes[0].operation==Cancel);
 assert(!resolveMouseMatches(keys,0,buttons,c,0,true,routes));
 c={0,0,0,0,17,17,0,0,1023};keys[17]=1;buttons[MouseAirFirst]=buttons[MouseAirSecond]=2;
 assert(resolveMouseMatches(keys,2,buttons,c,0,true,routes)==2);assert((routes[0].bindings&AirFirst)&&(routes[1].bindings&AirSecond));
 assert(resolveMouseMatches(keys,2,buttons,c,0,false,routes)==1&&(routes[0].bindings&AirFirst));
 Intent i{};i.tick=100;i.match=8;i.house=0x10000;i.frame=5;i.inputSerial=13;i.count=MaxSources;
 for(int j=0;j<MaxSources;++j)i.sources[j]={uint32_t(j+1),uint32_t(0x10000+4096*j)};
 PendingMatches pending;pending.begin(i,routes,2,7);assert(pending.waiting());auto first=pending.next(),second=pending.next();
 assert(first.bindings&BatchStart);assert(!(first.bindings&BatchNext));assert(second.bindings&BatchNext);assert(sameCapture(first,second));assert(!pending.waiting());
 auto changed=second;++changed.sources[MaxSources-1].id;assert(!sameCapture(first,changed));changed=second;++changed.tick;assert(!sameCapture(first,changed));
 changed=second;changed.count=MaxSources+1;assert(!sameCapture(changed,changed));assert(!pending.next().operation);
 pending.begin(i,routes,MouseCount+2,7);assert(!pending.active());pending.clear();
 std::cout<<"PASS: one OS registration/all matched keyboard actions, bounded FIFO/identity/deadline; all ten mouse routes, default precedence, both aircraft modes and anchored batch capture\n";
}
