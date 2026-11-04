#include "../../external_reader/source/cooperative_reader.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
struct Memory {
 std::vector<uint8_t> data=std::vector<uint8_t>(0xc00000);
 template<class T>void put(uint32_t p,T value){assert(uint64_t(p)+sizeof value<=data.size());memcpy(data.data()+p,&value,sizeof value);}
 void vec(uint32_t at,uint32_t items,const std::vector<uint32_t>&values){put(at+4,items);put<int>(at+8,values.size());put<int>(at+16,values.size());for(auto v:values){put(items,v);items+=4;}}
 bool read(uint32_t p,void*out,size_t n){if(p<0x10000||uint64_t(p)+n>data.size())return false;memcpy(out,data.data()+p,n);return true;}
};
struct Read {Memory*m;bool operator()(uint32_t p,void*out,size_t n)const{return m->read(p,out,n);}};
int main(){
 using namespace assist;Memory m;Read read{&m};external::GameView<Read> game(read);
 constexpr uint32_t house=0x400000,enemy=0x420000,ally=0x440000,neutral=0x460000,country=0x480000;
 constexpr uint32_t source=0x200000,sourceType=0x300000,targetType=0x310000,armorType=0x320000,protectedType=0x330000;
 constexpr uint32_t primary=0x500000,secondary=0x501000,elite=0x502000,stage=0x503000,ifv=0x504000,projectile=0x510000,wh=0x520000,wh2=0x521000;
 constexpr int center=25600;
 m.put(0xa83d4c,house);m.put<int>(0xa8ed84,100);m.put<uint8_t>(0xa8e9a0,1);m.put<int>(enemy+0x30,1);m.put<int>(ally+0x30,2);m.put<uint32_t>(house+0x5788,1u<<2);m.put(neutral+0x34,country);m.put<uint8_t>(country+0x1a6,1);
 auto type=[&](uint32_t t,int armor){m.put<int>(t+0x9c,armor);m.put<uint8_t>(t+0x231,1);m.put<uint8_t>(t+0xd3a,1);m.put<uint8_t>(t+0xd38,1);m.put<int>(t+0x684,-1);};
 type(sourceType,0);type(targetType,0);type(armorType,1);type(protectedType,0);m.put<uint8_t>(protectedType+0x233,1);
 std::vector<uint32_t> all;
 auto object=[&](uint32_t p,uint32_t vt,uint32_t t,uint32_t id,uint32_t owner,int x,int y){m.put(p,vt);m.put(p+0x10,id);m.put(p+(vt==0x7e3ebc?0x520:vt==0x7eb058?0x6c0:0x6c4),t);m.put(p+0x21c,owner);m.put<int>(p+0x6c,100);m.put<uint8_t>(p+0x74,1);m.put<uint8_t>(p+0x90,1);m.put<int>(p+0x9c,x);m.put<int>(p+0xa0,y);m.put<double>(p+0x158,1);m.put<double>(p+0x160,1);all.push_back(p);};
 object(source,0x7e22a4,sourceType,1,house,center-256,center);m.put<uint8_t>(source+0x83,1);
 auto target=[&](int id,int dx,int dy,uint32_t owner=0,uint32_t vt=0x7f5c70,uint32_t t=0){if(!owner)owner=enemy;if(!t)t=targetType;uint32_t p=0x220000+(id-100)*0x2000;object(p,vt,t,id,owner,center+dx,center+dy);return p;};
 uint32_t clicked=target(100,0,0),boundary=target(101,1280,0);target(102,1280,1);target(103,1024,1024);target(104,0,0,enemy,0x7e3ebc);target(105,0,0,enemy,0x7eb058);target(106,0,0,ally);target(107,0,0,neutral);
 uint32_t flying=target(108,0,0);m.put<uint8_t>(flying+0x134,1);uint32_t manipulated=target(109,0,0);m.put<uint32_t>(manipulated+0x2b0,0xdead);
 target(110,0,0,enemy,0x7f5c70,protectedType);uint32_t cloaked=target(111,0,0);m.put<int>(cloaked+0x220,1);target(112,0,0,enemy,0x7f5c70,armorType);uint32_t height=target(113,0,-1280);m.put<int>(height+0xa4,100000);
 auto weapon=[&](uint32_t w,int damage,uint32_t warhead){m.put<int>(w+0x9c,1);m.put(w+0xa0,projectile);m.put<int>(w+0xa4,damage);m.put(w+0xac,warhead);m.put<int>(w+0xb4,2560);};
 m.put<uint8_t>(projectile+0x2a5,1);m.put<double>(wh+0xa0,1);m.put<double>(wh2+0xa0,1);
 weapon(primary,40,wh);weapon(secondary,90,wh2);weapon(elite,70,wh);weapon(stage,130,wh);weapon(ifv,110,wh);m.put(sourceType+0x898,primary);m.vec(0xa8ec78,0x110000,all);
 cooperative::Request r{};r.serial=r.match=1;r.mode=cooperative::AirDamage;r.target=100;r.type=targetType;r.count=1;r.viewWidth=r.viewHeight=1;r.cameraX=r.cameraY=center;r.centerObject=clicked;r.sources[0]={1,7};
 auto input=[&](){return external::cooperativeInput(read,r,[](uint32_t){return cooperative::Quota{};});};
 auto w=input();std::set<uint32_t> candidates;for(auto&t:w.targets)candidates.insert(t.id);assert(candidates==std::set<uint32_t>({100,101,110,111,112,113}));
 auto orders=cooperative::allocate(w);assert(orders.size()==1&&orders[0].target==100&&orders[0].expectedDamage==40);
 assert(w.expectedDamage.size()==w.compatible.size());for(size_t j=0;j<w.targets.size();++j)if(w.targets[j].id==110||w.targets[j].id==111||w.targets[j].id==112)assert(!w.compatible[j]&&w.expectedDamage[j]==0);
 // The original point remains the circle centre after the clicked object moves.
 m.put<int>(clicked+0x9c,center+1000);w=input();assert(std::any_of(w.targets.begin(),w.targets.end(),[](auto&t){return t.id==113;}));m.put<int>(clicked+0x9c,center);
 m.put<uint32_t>(boundary+0x21c,house);w=input();assert(std::none_of(w.targets.begin(),w.targets.end(),[](auto&t){return t.id==101;}));m.put(boundary+0x21c,enemy);
 r.centerObject=boundary;assert(input().units.empty());r.centerObject=clicked;
 auto damage=[&](){auto a=cooperative::allocate(input());return a.empty()?0:a[0].expectedDamage;};
 double exactVerses=1.000000000123456;m.put<double>(wh+0xa0,exactVerses);assert(damage()==40*exactVerses&&damage()!=40*double(float(exactVerses)));m.put<double>(wh+0xa0,1);
 m.put(sourceType+0xa94,elite);m.put<float>(source+0x150,2);assert(damage()==70);m.put<float>(source+0x150,0);
 m.put(sourceType+0x898+28,secondary);m.put<double>(wh+0xa0,0);assert(damage()==90);m.put<double>(wh+0xa0,1);
 m.put<int>(sourceType+0x808,1);m.put<int>(source+0x138,3);m.put(sourceType+0x898+3*28,ifv);assert(damage()==110);m.put<int>(sourceType+0x808,0);
 m.put<uint8_t>(sourceType+0xcd5,1);m.put<int>(source+0x140,1);m.put(sourceType+0x898+2*28,stage);assert(damage()==130);m.put<uint8_t>(sourceType+0xcd5,0);
 m.put<int>(primary+0x9c,2);assert(damage()==80);m.put<int>(sourceType+0x684,4);m.put<int>(source+0x2fc,1);assert(damage()==40);m.put<int>(source+0x2fc,0);assert(damage()==0);m.put<int>(source+0x2fc,2);m.put<int>(source+0x3b8,1);assert(damage()==0);m.put<int>(source+0x3b8,0);m.put<int>(primary+0x9c,1);
 m.put<double>(source+0x160,2);m.put<double>(clicked+0x158,4);assert(damage()==20);m.put<double>(source+0x160,std::numeric_limits<double>::quiet_NaN());assert(damage()==0);m.put<double>(source+0x160,1);m.put<double>(clicked+0x158,1);
 m.put<int>(primary+0xa4,-40);m.put(sourceType+0x898+28,uint32_t(0));assert(damage()==0);m.put<int>(primary+0xa4,40);m.put<uint8_t>(wh+0x155,1);assert(damage()==0);m.put<uint8_t>(wh+0x155,0);
 // Single-target mode accepts an ordinary building and never adds neighbours.
 r.mode=cooperative::AirSingle;r.target=104;r.centerObject=0x228000;w=input();assert(w.targets.size()==1&&w.targets[0].id==104);assert(cooperative::allocate(w).size()==1);
 r.mode=cooperative::AirFirst;r.target=100;r.centerObject=clicked;assert(input().targets.empty()&&"legacy viewport semantics remain unchanged");
 std::cout<<"PASS production cooperative reader: frozen circular bounds, screen independence, target/ownership protection, selected/elite/IFV/staged weapons, exact Verses, ammo/burst/modifiers and single-target isolation\n";
}
