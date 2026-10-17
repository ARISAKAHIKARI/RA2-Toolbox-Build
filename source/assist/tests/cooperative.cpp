#include "../source/cooperative_plan.h"
#include "../source/additional_settings.h"
#include "../source/pointer_intent.h"
#include <cassert>
#include <map>
#include <iostream>
using namespace assist::cooperative;
Input make(int n,int targets,int mode=AirFirst){Input w;w.mode=mode;w.clicked=100;for(int i=0;i<n;++i)w.units.push_back({{uint32_t(i+1),7},1,0,0,1});for(int i=0;i<targets;++i)w.targets.push_back({uint32_t(100+i),2,i*256,0,3,0,{1,2,0}});w.compatible.assign(n*targets,1);return w;}
std::map<uint32_t,int> counts(const std::vector<Assignment>&a){std::map<uint32_t,int> out;for(auto&o:a)++out[o.target];return out;}
int main(){auto w=make(7,3);auto a=allocate(w);auto n=counts(a);assert(a.size()==7&&n[100]==3&&n[101]==2&&n[102]==2);w.mode=AirSecond;w.units.resize(6);w.compatible.resize(18);n=counts(allocate(w));assert(n[100]==2&&n[101]==2&&n[102]==2);
 w=make(10,3);w.targets[0].quota={2,3,2};w.targets[1].quota={1,2,1};w.targets[2].quota={0,0,0};n=counts(allocate(w));assert(n[100]==2&&n[101]==1&&n[102]==7);for(auto&t:w.targets)t.quota.maximum=1;assert(allocate(w).size()==3);
 w=make(2,2);w.compatible={1,1,1,0};n=counts(allocate(w));assert(n[100]==1&&n[101]==1);
 w=make(3,2,Enter);w.units[0].slots=3;w.units[1].slots=w.units[2].slots=2;w.targets[0].free=4;w.targets[1].free=3;a=allocate(w);assert(a.size()==3);std::map<uint32_t,int> slots;for(auto&o:a)slots[o.target]+=w.units[o.id-1].slots;assert(slots[100]<=4&&slots[101]<=3);
 w.compatible.pop_back();assert(allocate(w).empty());Request r{};r.serial=r.match=1;r.mode=Enter;r.count=MaxSources+1;assert(!valid(r));r.count=1;r.viewWidth=1920;r.viewHeight=1080;assert(valid(r));
 assist::AdditionalSettings defaults;assert(defaults.enterKey==18&&defaults.airKey1==17&&!defaults.airKey2&&defaults.defaults.first==1&&defaults.defaults.second==2&&!defaults.defaults.maximum);
 {
 using namespace assist::pointerinput;uint8_t keys[256]{};
 Bindings c{0,0,0,0,0,0,0,0,1023};
 auto b=resolveBindings(keys,Enhanced,c);
 assert((b&(Advanced|Magnetron|CS|assist::pointerinput::Enter|assist::pointerinput::AirFirst|Control))==(Advanced|Magnetron|CS|assist::pointerinput::Enter|assist::pointerinput::AirFirst|Control));assert(!(b&assist::pointerinput::AirSecond));
 assert(resolveBindings(keys,Sell,c)&Control);
 assert(resolveBindings(keys,AttackMove,c)&Advanced);
 assert(resolveBindings(keys,FollowNative,c)==0&&resolveBindings(keys,Cancel,c)==0);
 for(int vk:{16,17,18}){keys[vk]=1;assert((resolveBindings(keys,Enhanced,c)&~Shift)==0);keys[vk]=0;}
 c.cs='G';keys['G']=1;b=resolveBindings(keys,Enhanced,c);assert((b&(Advanced|Magnetron|CS))==CS);keys['G']=0;
 c.airFirst=17;keys[17]=1;b=resolveBindings(keys,Enhanced,c);assert((b&(assist::pointerinput::AirFirst|assist::pointerinput::AirSecond))==assist::pointerinput::AirFirst);keys[17]=0;
 b=resolveBindings(keys,Enhanced,c);assert((b&(assist::pointerinput::AirFirst|assist::pointerinput::AirSecond))==assist::pointerinput::AirSecond);
 c.airFirst=0;c.airSecond='G';keys['G']=1;b=resolveBindings(keys,Enhanced,c);assert((b&(assist::pointerinput::AirFirst|assist::pointerinput::AirSecond))==assist::pointerinput::AirSecond);keys['G']=0;
 c.disk=c.sell=17;assert(!(resolveBindings(keys,Enhanced,c)&Control)&&!(resolveBindings(keys,Sell,c)&Control));keys[17]=1;assert(resolveBindings(keys,Sell,c)&Control);
 c.sell='H'|(2<<8);assert(!(resolveBindings(keys,Sell,c)&Control));keys['H']=1;assert(resolveBindings(keys,Sell,c)&Control);keys['H']=keys[17]=0;
 c.enabled=0;assert(resolveBindings(keys,Enhanced,c)==0&&resolveBindings(keys,Sell,c)==0);
 // All ten mouse slots use the production router; absent/disabled bindings
 // cannot borrow another feature's button. Same-button ground behavior stays.
 uint32_t buttons[MouseCount]{};Bindings independent{0,0,0,0,0,0,0,0,1023};
 const uint32_t expected[]={Advanced,Magnetron,CS,assist::pointerinput::Enter,assist::pointerinput::AirFirst,assist::pointerinput::AirSecond,Control,0,Control|SellDrainedInput,Control|SellVehicleInput};
 for(int slot=0;slot<MouseCount;++slot)for(int button=0;button<=4;++button){
  for(auto&v:buttons)v=0;buttons[slot]=button;
  for(int physical=1;physical<=4;++physical){
   auto got=resolveMouse(keys,physical,buttons,independent,SellDrainedInput|SellVehicleInput);
   if(button&&physical==button){assert(got.operation==(slot<MouseAttack?Enhanced:slot==MouseAttack?AttackMove:Sell));assert((got.bindings&~(RightClick|PerFeatureMouse))==expected[slot]);assert(bool(got.bindings&RightClick)==(physical==4));}
   else assert(got.operation==(physical==4?FollowNative:0));
  }
 }
 for(auto&v:buttons)v=0;buttons[MouseAdvanced]=1;buttons[MouseMagnetron]=2;buttons[MouseCS]=3;
 for(int n=1;n<=3;++n)assert((resolveMouse(keys,n,buttons,independent,0).bindings&7)==unsigned(1<<(n-1)));
 buttons[MouseMagnetron]=1;assert((resolveMouse(keys,1,buttons,independent,0).bindings&7)==3);
 independent.enabled=0;assert(!resolveMouse(keys,1,buttons,independent,0).operation);
 assert(resolveMouse(keys,5,buttons,independent,0).operation==Cancel);
 independent.enabled=1023;buttons[MouseSellVehicle]=3;independent.sell=17;
 assert(resolveMouse(keys,3,buttons,independent,SellVehicleInput).operation==Enhanced); // CS still configured
 buttons[MouseCS]=0;assert(!resolveMouse(keys,3,buttons,independent,SellVehicleInput).operation);
 keys[17]=1;assert(resolveMouse(keys,3,buttons,independent,SellVehicleInput).operation==Sell);keys[17]=0;
 independent.sell=0;assert(!resolveMouse(keys,3,buttons,independent,SellDrainedInput).operation);
 independent.cs=0;buttons[MouseCS]=4;auto right=resolveMouse(keys,4,buttons,independent,0);assert(right.operation==Enhanced&&(right.bindings&(CS|RightClick))==(CS|RightClick));

 }
 std::cout<<"PASS: both aircraft priorities, per-target caps/unlimited, AA/AG scarcity, garrison size best-fit, malformed request/viewport guards and defaults\n";
}
