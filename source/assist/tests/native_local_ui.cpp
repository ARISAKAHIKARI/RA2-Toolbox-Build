#include "../../external_reader/source/native_local_ui.h"
#include <cassert>
#include <iostream>
int main(){
 std::vector<uint8_t>memory(0xC00000);auto put=[&](uint32_t p,auto value){std::memcpy(memory.data()+p,&value,sizeof value);};auto read=[&](uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size())return false;std::memcpy(out,memory.data()+p,n);return true;};
 auto text=[&](uint32_t p,const char*s){for(;*s;++s,p+=2)put(p,uint16_t(uint8_t(*s)));put(p,uint16_t(0));};
 external::mainview::View view{0x20000,0x21000,0x22000,50,100,400,800,600,0,0,640,600};std::vector<external::nativefeedback::Rect>rects;
 assert(external::localui::capture(read,view,1280,1200,rects)&&rects.empty());
 constexpr uint32_t label=0x30000,font=0x40000,internal=0x41000,message=0x42000;put(0xA8BC60,label);put(label,0x7F5B44u);put(label+0xC,40);put(label+0x10,12);put(label+0x30,message);text(message,"Local only message");put(0x89C4D0,font);put(font+4,internal);put(internal,8);put(font+0x1C,12);
 assert(external::localui::capture(read,view,1280,1200,rects)&&rects.size()==1&&rects[0].y==20&&rects[0].h==34);rects.clear();put(label+0x40,uint8_t(1));assert(external::localui::capture(read,view,1280,1200,rects)&&rects.empty());put(label+0x40,uint8_t(0));
 put(label+4,label);assert(!external::localui::capture(read,view,1280,1200,rects)&&rects.empty());put(label+4,0u);put(label+0xC,INT32_MIN);assert(!external::localui::capture(read,view,1280,1200,rects));put(label+0xC,40);put(0xA8BC60,0u);
 constexpr uint32_t super=0x50000,type=0x51000,title=0x52000,items=0x53000;put(0xA83D54,items);put(0xA83D58,1);put(0xA83D60,1);put(items,super);put(super,0x7F3FE8u);put(super+0x28,type);put(super+0x30,10);put(super+0x38,600);put(type+0x60,title);text(title,"Nuclear Missile");
 assert(external::localui::capture(read,view,640,600,rects)&&rects.size()==1&&rects[0].y==582&&rects[0].h==18);rects.clear();put(view.scenario+0x11E8,5);put(view.scenario+0x11F0,150);put(view.scenario+0x11F4,title);assert(external::localui::capture(read,view,640,600,rects)&&rects[0].y==568);rects.clear();
 constexpr uint32_t factory=0x60000,factoryType=0x61000,cell=0x90000,cells=0xA0000;put(factory,0x7E3EBCu);put(factory+0x10,13u);put(factory+0x520,factoryType);put(factory+0x21C,view.house);put(factory+0x6C,500);put(factory+0x74,uint8_t(1));put(factory+0x90,uint8_t(1));put(factory+0x3D3,uint8_t(1));
 external::nativefeedback::Object object{factory,13,factoryType,view.house,{200,100,80,160},3};
 assert(external::localui::buildings(read,view,{object},640,600,rects)&&rects.size()==1&&rects[0].x==168&&rects[0].y==68);rects.clear();put(factory+0x3D3,uint8_t(0));put(factory+0x83,uint8_t(1));put(factory+0x218,cell);put(cell+0x24,int16_t(30));put(cell+0x26,int16_t(25));put(cell+0x11B,int8_t(2));put(cells+(25*512+30)*4,cell);
 put(external::mapbounds::Map+0xF4,32);put(external::mapbounds::Map+0xF8,64);put(external::mapbounds::Map+0x104,32);put(external::mapbounds::Map+0x108,64);put(external::mapbounds::Map+0x13C,cells);put(external::mapbounds::Map+0x140,512*512);
 assert(external::localui::buildings(read,view,{object},640,600,rects)&&!rects.empty());bool flag=false;for(auto r:rects)flag|=r.x<=50&&r.y<=410&&r.x+r.w>50&&r.y+r.h>410;assert(flag);rects.clear();
 object.owner+=4;assert(external::localui::buildings(read,view,{object},640,600,rects)&&rects.empty());object.owner=view.house;put(factory+0x10,14u);assert(external::localui::buildings(read,view,{object},640,600,rects)&&rects.empty());
 std::cout<<"PASS main-only native messages, SW/mission timer bands, primary factory fallback, verified rally cell, cycle/pointer/owner rejection and physical scaling; native pixels not certified\n";
}
