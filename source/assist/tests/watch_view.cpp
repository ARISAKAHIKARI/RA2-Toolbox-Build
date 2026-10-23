#include "../../external_reader/source/watch_view.h"
#include <cassert>
#include <iostream>
using namespace external;
int main(){
 std::vector<uint8_t> memory(0x1000000);uint32_t next=0x10000;
 auto alloc=[&](uint32_t n){uint32_t p=next;next+=(n+15)&~15;return p;};
 auto write=[&](uint32_t p,auto value){memcpy(memory.data()+p,&value,sizeof value);};
 auto read=[&](uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size())return false;memcpy(out,memory.data()+p,n);return true;};
 auto vec=[&](uint32_t p,std::initializer_list<uint32_t> items){uint32_t data=alloc(items.size()*4);unsigned i=0;for(auto v:items)write(data+4*i++,v);write(p+4,data);write(p+8,int(items.size()));write(p+16,int(items.size()));};
 uint32_t local=alloc(0x20000),enemy=alloc(0x20000),carrierType=alloc(0x2000),engineerType=alloc(0x2000);
 write(0xA83D4C,local);write(enemy+0x30,1);write(engineerType+0xEC3,uint8_t(1));
 auto obj=[&](uint32_t type,bool infantry,int id,int x,int y){uint32_t p=alloc(0x800);write(p,infantry?0x7EB058u:0x7F5C70u);write(p+0x10,id);write(p+0x6C,100);write(p+0x74,uint8_t(1));write(p+0x90,uint8_t(1));write(p+0x21C,enemy);write(p+(infantry?0x6C0:0x6C4),type);write(p+0x9C,x*256+128);write(p+0xA0,y*256+128);return p;};
 uint32_t a=obj(carrierType,false,10,20,30),b=obj(carrierType,false,11,25,35),cargo=obj(engineerType,true,12,20,30),far=obj(carrierType,false,13,26,30),hidden=obj(carrierType,false,14,20,31);
 write(a+0x114,1);write(a+0x118,cargo);write(cargo+0x11C,a);write(hidden+0x220,1);vec(0xA8EC78,{a,b,cargo,far,hidden});vec(0xA8ECB8,{b});
 watch::Target ta{a,10},tb{b,11};assert(watch::choose(read,{}, {},true)==ta);assert(watch::choose(read,tb,ta,true)==tb);assert(!watch::choose(read,{},ta,false));
 assert(watch::manual(read)==tb);write(a+0x431,uint8_t(1));assert(watch::manual(read)==ta);write(a+0x431,uint8_t(0));
 uint32_t cells=alloc(512*512*4);write(radar::Map+0x13C,cells);write(radar::Map+0x140,512*512);
 for(int y=25;y<=35;++y)for(int x=15;x<=25;++x){uint32_t p=alloc(0x148);write(cells+(y*512+x)*4,p);write(p+0x24,int16_t(x));write(p+0x26,int16_t(y));write(p+0x120,int8_t(-1));write(p+0x121,int8_t(-1));}
 auto frame=watch::sample(read,ta,false);assert(frame.focus==ta&&frame.cx==20&&frame.cy==30&&frame.dangerous&&frame.passengers.size()==1&&frame.actors.size()==2);
 assert(std::all_of(std::begin(frame.cells),std::end(frame.cells),[](auto c){return c!=0;}));
 uint32_t center=0;read(cells+(30*512+20)*4,&center,4);write(center+0x120,int8_t(0));frame=watch::sample(read,ta,false);assert(frame.cells[60]==0&&frame.actors.size()==1);frame=watch::sample(read,ta,true);assert(frame.cells[60]&&frame.actors.size()==2);
 // Stable identities: a recycled address must not inherit the pinned unit.
 write(a+0x10,99u);assert(!watch::choose(read,ta,{},true));assert(!watch::sample(read,ta,true).focus);write(a+0x10,10u);
 write(cargo+0x30,cargo);assert(!watch::choose(read,{},ta,true));write(cargo+0x30,0u);
 write(a+0x6C,0);assert(!watch::choose(read,{},ta,true));write(a+0x6C,100);
 write(engineerType+0xEC3,uint8_t(0));assert(!watch::choose(read,{},ta,true));
 // Native cell +38/+11A/+11B and overlay +44/+11E fields, separate terrain array.
 uint32_t tileType=alloc(0x400),overlayType=alloc(0x400),tree=alloc(0x100),treeType=alloc(0x400);
 vec(0xA8ED28,{tileType});vec(0xA83D80,{overlayType});write(tileType+0xA4,0x30000u);write(overlayType+0xA4,0x31000u);
 write(center+0x38,0);write(center+0x11A,uint8_t(2));write(center+0x11B,uint8_t(3));write(center+0x44,0);write(center+0x11E,uint8_t(1));
 write(tree,0x7F522Cu);write(tree+0x10,20u);write(tree+0x6C,100);write(tree+0x9C,20*256+128);write(tree+0xA0,30*256+128);write(tree+0xC8,treeType);write(tree+0xAC,1);vec(0xA8E988,{tree});
 frame=watch::sample(read,ta,true);assert(frame.tiles[60].image==0x30000&&frame.tiles[60].sub==2&&frame.tiles[60].level==3&&frame.tiles[60].overlay==0x31000&&frame.tiles[60].overlayFrame==1);
 assert(frame.actors.size()==3&&std::any_of(frame.actors.begin(),frame.actors.end(),[&](const auto&a){return a.kind==4&&a.stage==1&&a.type==treeType;}));
 write(tree+0xCD,uint8_t(1));assert(watch::sample(read,ta,true).actors.size()==2);
 write(radar::Map+0x140,512*512+1);assert(!watch::sample(read,ta,true).focus);
 std::cout<<"PASS automatic dangerous carrier, hover/selection pin, 121 cells, range edge, fog, passengers/cloak exclusion, dead/recycled target, corrupt list and map bounds\n";
}
