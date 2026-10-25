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
 write(radar::Map+0xF4,32);write(radar::Map+0xF8,64);write(radar::Map+0x104,32);write(radar::Map+0x108,64);
 write(0xB0CD48,.144337567);write(0x89E7C0,104);
 for(int y=25;y<=35;++y)for(int x=15;x<=25;++x){uint32_t p=alloc(0x148);write(cells+(y*512+x)*4,p);write(p+0x24,int16_t(x));write(p+0x26,int16_t(y));write(p+0x120,int8_t(-1));write(p+0x121,int8_t(-1));write(p+0x12C,uint8_t(0x10));write(p+0x140,uint8_t(1));}
 auto frame=watch::sample(read,ta,false);assert(frame.focus==ta&&frame.cx==20&&frame.cy==30&&frame.dangerous&&frame.passengers.size()==1&&frame.actors.size()==2);
 auto cellIn=[&](const watch::Frame&f,int x,int y){int ix=x-f.cx+f.radius,iy=y-f.cy+f.radius;return ix<0||iy<0||ix>=f.side||iy>=f.side?0u:f.cells[iy*f.side+ix];};
 assert(std::all_of(std::begin(frame.cells),std::end(frame.cells),[](auto c){return c!=0;}));
 uint32_t center=0;read(cells+(30*512+20)*4,&center,4);
 // Cached draw-frame bytes can disagree with the live neighbor flags; derive
 // the mask from the native lookup, including an edge frame past 15.
 write(0x7F4194+255,int8_t(46));write(center+0x120,int8_t(-2));write(center+0x121,int8_t(-2));write(center+0x12C,uint8_t(0x18));
 frame=watch::sample(read,ta,false);assert(frame.cells[60]&&frame.actors.size()==2&&frame.shade[60]==(radar::ShroudFrames{46,-1}));
 write(center+0x12C,uint8_t(0));frame=watch::sample(read,ta,false);assert(frame.cells[60]==0&&frame.actors.size()==1&&frame.shade[60].shroud==-2);
 write(0x7F4194+255,int8_t(127));frame=watch::sample(read,ta,false);assert(!frame.focus);write(0x7F4194+255,int8_t(46));
 frame=watch::sample(read,ta,true);assert(frame.cells[60]&&frame.actors.size()==2&&frame.shade[60]==(radar::ShroudFrames{-1,-1}));
 // Stable identities: a recycled address must not inherit the pinned unit.
 write(a+0x10,99u);assert(!watch::choose(read,ta,{},true));assert(!watch::sample(read,ta,true).focus);write(a+0x10,10u);
 write(cargo+0x30,cargo);assert(!watch::choose(read,{},ta,true));write(cargo+0x30,0u);
 write(a+0x6C,0);assert(!watch::choose(read,{},ta,true));write(a+0x6C,100);
 write(engineerType+0xEC3,uint8_t(0));assert(!watch::choose(read,{},ta,true));
 // Native cell +38/+11A/+11B and overlay +44/+11E fields, separate terrain array.
 uint32_t tileType=alloc(0x400),overlayType=alloc(0x400),tree=alloc(0x100),treeType=alloc(0x400);
 vec(0xA8ED28,{tileType});vec(0xA83D80,{overlayType});write(tileType+0xA4,0x30000u);write(overlayType+0xA4,0x31000u);
 write(center+0x38,0);write(center+0x11A,uint8_t(2));write(center+0x11B,uint8_t(3));write(center+0x44,0);write(center+0x11E,uint8_t(1));
 write(a+0xA4,312); // Put the focus on its level-3 terrain, not below that floor.
 write(tree,0x7F522Cu);write(tree+0x10,20u);write(tree+0x6C,100);write(tree+0x9C,20*256+128);write(tree+0xA0,30*256+128);write(tree+0xC8,treeType);write(tree+0xAC,1);vec(0xA8E988,{tree});
 frame=watch::sample(read,ta,true);assert(frame.tiles[60].image==0x30000&&frame.tiles[60].sub==2&&frame.tiles[60].level==3&&frame.tiles[60].overlay==0x31000&&frame.tiles[60].overlayFrame==1);
 assert(frame.actors.size()==3&&std::any_of(frame.actors.begin(),frame.actors.end(),[&](const auto&a){return a.kind==4&&a.stage==1&&a.type==treeType;}));
 write(tree+0xCD,uint8_t(1));assert(watch::sample(read,ta,true).actors.size()==2);
 // A mission trigger changes only VisibleRect, with the same cells, focus,
 // frame counter and match. The next refresh must reject the old domain.
 write(radar::Map+0x100,9);write(radar::Map+0x108,55);
 assert(!watch::refresh(read,frame));auto reduced=watch::sample(read,ta,true,20);
 assert(reduced.focus==ta&&cellIn(reduced,20,30)&&reduced.bounds.top==9&&"camera bounds must not discard drawable non-usable border cells");
 write(radar::Map+0x100,0);write(radar::Map+0x108,64);
 auto expanded=watch::sample(read,ta,true);assert(cellIn(expanded,20,30)&&expanded.bounds!=reduced.bounds);
 // A readable but physically out-of-map cell must stay hidden even in reveal.
 write(radar::Map+0xF4,60);write(radar::Map+0x104,60);
 auto outside=watch::sample(read,ta,true);assert(outside.focus==ta&&!cellIn(outside,20,30));
 // Native camera clamping at a map corner moves the view away from its
 // target. Sample around that clamped center, otherwise 12 visible cells in
 // this case fall outside the old target-centered radius-20 inventory.
 write(radar::Map+0xF4,131);write(radar::Map+0xF8,146);write(radar::Map+0xFC,3);write(radar::Map+0x100,5);write(radar::Map+0x104,124);write(radar::Map+0x108,135);
 write(a+0x9C,9*256+128);write(a+0xA0,133*256+128);write(a+0xA4,0);
 for(int y=100;y<=170;++y)for(int x=1;x<=60;++x){uint32_t p=alloc(0x148);write(cells+(y*512+x)*4,p);write(p+0x24,int16_t(x));write(p+0x26,int16_t(y));write(p+0x120,int8_t(-1));write(p+0x121,int8_t(-1));}
 auto corner=watch::sample(read,ta,true,20);assert(corner.focus==ta&&corner.focusCx==9&&corner.focusCy==133&&corner.cameraX==-3430&&corner.cameraY==2296);
 int beyondOldInventory=0;
 for(int y=100;y<=170;++y)for(int x=1;x<=60;++x){double px=160+((x-y)*30-corner.cameraX)*.5,py=128+((x+y+1)*15-corner.cameraY)*.5;
  if(!corner.bounds.legal(x,y)||px<0||px>=320||py<0||py>=256)continue;
  int ix=x-corner.cx+corner.radius,iy=y-corner.cy+corner.radius;
  assert(ix>=0&&ix<corner.side&&iy>=0&&iy<corner.side&&corner.cells[iy*corner.side+ix]);
  if(std::abs(x-corner.focusCx)>20||std::abs(y-corner.focusCy)>20)++beyondOldInventory;
 }
 assert(beyondOldInventory>=12&&watch::refresh(read,corner));
 // The top playable edge is below the camera viewport edge. Render legal
 // scenery in that margin while the focus stays above the clamped camera center.
 write(a+0x9C,69*256+128);write(a+0xA0,73*256+128);
 for(int y=45;y<=105;++y)for(int x=40;x<=100;++x){uint32_t p=alloc(0x148);write(cells+(y*512+x)*4,p);write(p+0x24,int16_t(x));write(p+0x26,int16_t(y));write(p+0x120,int8_t(-1));write(p+0x121,int8_t(-1));}
 auto topEdge=watch::sample(read,ta,true,20);assert(topEdge.cameraY==2296&&topEdge.bounds.usable(69,73));
 double focusY=128+((69+73+1)*15-topEdge.cameraY)*.5;assert(focusY>0&&focusY<128);
 int renderedMargin=0;for(int y=45;y<=105;++y)for(int x=40;x<=100;++x){double px=160+((x-y)*30-topEdge.cameraX)*.5,py=128+((x+y+1)*15-topEdge.cameraY)*.5;
  if(px<0||px>=320||py<0||py>=256||!topEdge.bounds.legal(x,y))continue;assert(cellIn(topEdge,x,y));if(!topEdge.bounds.usable(x,y))++renderedMargin;
 }assert(renderedMargin>0&&"drawable edge scenery must fill the margin above the playable boundary");
 write(radar::Map+0x140,512*512+1);assert(!watch::sample(read,ta,true).focus);
 std::cout<<"PASS automatic dangerous carrier, hover/selection pin, 121 cells, range edge, fog, passengers/cloak exclusion, dead/recycled target, corrupt list and map bounds\n";
}
