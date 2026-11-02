#include "../source/mo_patrol_graph.h"
#include "../source/constructor_adapter.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
using namespace assist;
static std::vector<uint8_t> memory(0x1000000);static uint32_t failAt=0;static int checks=0;
template<class T>static void put(uint32_t at,T v){std::memcpy(memory.data()+at,&v,sizeof v);}
static bool read(uint32_t at,void*out,size_t n){if(at<0x10000||uint64_t(at)+n>memory.size()||(failAt>=at&&failAt<uint64_t(at)+n))return false;std::memcpy(out,memory.data()+at,n);return true;}
static void check(bool b){++checks;assert(b);}
static constexpr uint32_t House=0x100000,Unit=0x110000,Other=0x111000,Token=0x120000,Foreign=0x121000,Items=0x130000;
static mopatrol::Actor actor{Unit,77};static uint32_t cells[]={20020,20024,24024,24020};
static void vector(uint32_t at,uint32_t items,int count){put(at+4,items);put(at+8,count);put(at+16,count);}
static void setup(bool path=true){std::fill(memory.begin(),memory.end(),0);failAt=0;
 put(Unit+0x10,77u);put(Unit+0x21C,House);put(Unit+0x514,path?Token:0u);vector(0xAC4C78,Items,path?1:0);put(Items,Token);
 if(!path)return;put(Token,Unit);vector(Token+4,Items+0x100,4);put(Token+0x8C,0);put(Token+0x90,-1);put(Token+0x94,-1);
 for(int i=0;i<4;++i){uint32_t node=0x140000+i*0x100,member=0x150000+i*0x100,event=0x160000+i*0x100;put(Items+0x100+i*4,node);vector(node,node+0x40,1);put(node+0x40,member);put(member,Unit);put(member+4,event);put<uint8_t>(event,4);put(event+7,77u);put<uint8_t>(event+11,0x34);put<uint8_t>(event+12,2);put(event+19,cells[i]);put<uint8_t>(event+23,11);put(event+24,cells[i]);put<uint8_t>(event+28,11);}
}
static void connect(int start=0){put(Token+0x90,4-start);put(Token+0x94,start);for(int i=start;i<4;++i)put<uint8_t>(0x160000+i*0x100+12,29);}
int main(){mopatrol::Path path,after;
 setup(false);check(mopatrol::empty(read,House,&actor,1));failAt=Unit+0x514;check(!mopatrol::empty(read,House,&actor,1));
 setup();check(!mopatrol::empty(read,House,&actor,1));check(mopatrol::route(read,House,actor,cells,4,path,false));
 check(!mopatrol::route(read,House,actor,cells,4,after,true,&path));connect();check(mopatrol::route(read,House,actor,cells,4,after,true,&path));put<uint8_t>(0x160000+12,2);check(!mopatrol::route(read,House,actor,cells,4,after,true,&path));connect();
 put(Token+0x90,3);put(Token+0x94,1);check(!mopatrol::route(read,House,actor,cells,4,after,true,&path));
 put(Token+0x90,4);put(Token+0x94,0);put(Token+0x8C,-1);check(!mopatrol::route(read,House,actor,cells,4,after,true,&path));
 setup();check(mopatrol::route(read,House,actor,cells,4,path,false));connect();put(0x160000+19,20021u);check(!mopatrol::route(read,House,actor,cells,4,after,true,&path));
 setup();check(mopatrol::route(read,House,actor,cells,4,path,false));connect();put(Unit+0x514,Token+0x200u);std::memcpy(memory.data()+Token+0x200,memory.data()+Token,0x9C);check(!mopatrol::route(read,House,actor,cells,4,after,true,&path));
 setup();put(0x160000+7,88u);check(!mopatrol::route(read,House,actor,cells,4,after,false));
 setup();put<uint8_t>(0x160000+12,1);check(!mopatrol::route(read,House,actor,cells,4,after,false));
 setup();put<uint8_t>(0x160000+23,0x34);check(!mopatrol::route(read,House,actor,cells,4,after,false));
 setup();failAt=0x160000+24;check(!mopatrol::route(read,House,actor,cells,4,after,false));
 setup();put(Unit+0x21C,House+0x1000u);check(!mopatrol::route(read,House,actor,cells,4,after,false));
 setup();put(Token,Other);check(!mopatrol::route(read,House,actor,cells,4,after,false));
 setup();vector(0xAC4C78,Items,2);put(Items+4,Foreign);put(Foreign,Other);put(Other+0x514,Foreign);put(Other+0x21C,House);put(Foreign+0x8C,0);check(!mopatrol::exclusive(read,House,&actor,1));put(Foreign+0x8C,-1);check(mopatrol::exclusive(read,House,&actor,1));failAt=Foreign+0x8C;check(!mopatrol::exclusive(read,House,&actor,1));failAt=0;put(Other+0x21C,House+0x1000u);check(mopatrol::exclusive(read,House,&actor,1));
 // A mutation between the first capture and validation pass must be rejected.
 setup();int calls=0;auto changing=[&](uint32_t at,void*out,size_t n){if(at==Token+0x8C&&++calls==2)put(Token+0x8C,-1);return read(at,out,n);};check(!mopatrol::route(changing,House,actor,cells,4,after,false));
 intent::EventOrder order{};order.domain=vanilla::Domain::Waypoints;order.player=0;order.eventKind=vanilla::EventType::PlanConnect;order.a=77;order.tagA=0x34;order.b=cells[0];order.tagB=11;
 check(constructors::admit(order)==constructors::Verdict::Ready);order.c=1;check(constructors::admit(order)==constructors::Verdict::InvalidFields);order.c=0;order.eventKind=vanilla::EventType::PlanCommit;check(constructors::admit(order)==constructors::Verdict::ProbeOnly);order.eventKind=vanilla::EventType::PlanConnect;order.b=20000;check(constructors::admit(order)==constructors::Verdict::InvalidFields);
 std::cout<<"PASS "<<checks<<" checked native patrol graph and narrow constructor cases\n";
}
