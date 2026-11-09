#include "../source/mo_orders.h"
#include "../source/mo_patrol_graph.h"
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using namespace assist;
static std::vector<uint8_t> memory(0x1000000);
static constexpr uint32_t House=0x100000,Unit=0x110000,Token=0x120000,Items=0x130000;
static mopatrol::Actor actor{Unit,77};
static int checks=0,failures=0;
static void check(bool ok,const char*label){++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<label<<'\n';}}
template<class T>static void put(uint32_t at,T value){std::memcpy(memory.data()+at,&value,sizeof value);}
static bool read(uint32_t at,void*out,size_t n){if(at<0x10000||uint64_t(at)+n>memory.size())return false;std::memcpy(out,memory.data()+at,n);return true;}
static void vectorAt(uint32_t at,uint32_t items,int count){put(at+4,items);put(at+8,count);put(at+16,count);}
// Model only the already-materialized native graph at the reader boundary.
// Production patrol geometry and complete route/identity checks are unchanged.
static void graph(const uint32_t*cells,int count,bool closed){
 std::fill(memory.begin(),memory.end(),0);
 put(Unit+0x10,77u);put(Unit+0x21C,House);put(Unit+0x514,Token);
 vectorAt(0xAC4C78,Items,1);put(Items,Token);put(Token,Unit);
 vectorAt(Token+4,Items+0x100,count);put(Token+0x8C,0);put(Token+0x90,closed?count:-1);put(Token+0x94,closed?0:-1);
 for(int i=0;i<count;++i){
  uint32_t node=0x140000+i*0x100,member=0x150000+i*0x100,event=0x160000+i*0x100;
  put(Items+0x100+i*4,node);vectorAt(node,node+0x40,1);put(node+0x40,member);put(member,Unit);put(member+4,event);
  put<uint8_t>(event,4);put(event+7,77u);put<uint8_t>(event+11,0x34);put<uint8_t>(event+12,closed?29:2);
  put(event+19,cells[i]);put<uint8_t>(event+23,11);put(event+24,cells[i]);put<uint8_t>(event+28,11);
 }
}
int main(int argc,char**argv){
 moorders::Options options;options.loopStep=2;
 if(argc==2&&std::string(argv[1])=="--small-buffer"){
  // Existing MO option tests own this 16-element destination. ASan verifies
  // the actual production write boundary rather than an equality of constants.
  moorders::Point stroke[]={{20,20},{60,20},{60,60},{20,60},{20,20}},out[16]{};
  const int count=moorders::patrol(stroke,5,out,options);
  check(count>=4&&count<=16,"real patrol writes fit the established sixteen-point buffer");
  int digest=0;for(int i=0;i<std::min(count,16);++i)digest+=out[i].x+out[i].y;
  std::cout<<"buffer count="<<count<<" digest="<<digest<<'\n';
 }else{
  for(int width:{2,4,8,10,20,40,80}){
   moorders::Point stroke[]={{20,20},{20+width,20},{20+width,20+width},{20,20+width},{20,20}},out[256]{};
   const int count=moorders::patrol(stroke,5,out,options);
   check(count>=4&&count<=16,"closed-stroke resampling stays inside the route admission limit before submission");
   uint32_t cells[256]{};int left=1000,right=-1,top=1000,bottom=-1;
   for(int i=0;i<count;++i){cells[i]=uint32_t(out[i].y*1000+out[i].x);left=std::min(left,out[i].x);right=std::max(right,out[i].x);top=std::min(top,out[i].y);bottom=std::max(bottom,out[i].y);}
   check(left==20&&right==20+width&&top==20&&bottom==20+width,"bounded resampling retains every side of the whole long loop");
   graph(cells,count,false);mopatrol::Path open,closed;
   const bool bound=mopatrol::route(read,House,actor,cells,count,open,false);
   check(bound,"generated materialized path passes the real pre-Connect route reader");
   graph(cells,count,true);
   check(mopatrol::route(read,House,actor,cells,count,closed,true,bound?&open:nullptr),"generated closed path passes the real pre-Commit route reader");
   std::cout<<"width="<<width<<" count="<<count<<" bind="<<bound<<'\n';
  }
  moorders::Point point{30,30},out[16]{};
  check(moorders::patrol(&point,1,out)==4,"single-click patrol still has four corners");
  moorders::Point open[]={{30,30},{60,40}};
  check(moorders::patrol(open,2,out)==4,"open-stroke patrol still has four corners");
  uint32_t cells[33]{};mopatrol::Path rejected;
  check(!mopatrol::route(read,House,actor,cells,17,rejected,false)&&!mopatrol::route(read,House,actor,cells,32,rejected,false),"existing larger-route rejection remains conservative");
 }
 std::cout<<(failures?"FAIL: ":"PASS: ")<<checks<<" checks, "<<failures<<" failures; real patrol to route, synthetic graph only\n";
 return failures?1:0;
}
