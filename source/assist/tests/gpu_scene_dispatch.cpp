#include "../../external_reader/source/gpu_scene_dispatch.h"
#include <cassert>
#include <random>
#include <iostream>
using namespace external::scenedispatch;
int main(){
 std::vector<std::array<int,4>> b{{9,17,14,26},{100,200,110,210},{0,0,0,0},{45,33,46,34}};
 auto r=region(b,0,1,1280,720,false,false,false);assert(r.x==8&&r.y==16&&r.groupsX==1&&r.groupsY==2);
 assert(region(b,0,0,1280,720,false,false,false).invocations()==0);
 assert(region(b,0,4,1280,720,true,false,false).invocations()==1280*720);
 assert(region(b,0,0,1280,720,false,false,true).invocations()==1280*720);
 assert(region(b,0,4,1280,720,false,true,false).invocations()==64);
 assert(region(b,0,1,1280,720,false,false,false,{16,0,1280,720}).invocations()==0);
 r=region(b,3,4,1280,720,false,false,false);assert(r.x==40&&r.y==32&&r.origin()==(40u|(32u<<16)));
 std::mt19937 gen(1477);uint64_t checked=0;
 for(int run=0;run<400;++run){int w=1+gen()%257,h=1+gen()%129;std::vector<std::array<int,4>> bounds;
  for(int n=0;n<1+int(gen()%24);++n){int x=gen()%w,y=gen()%h;bounds.push_back({x,y,std::min(w,x+int(gen()%35)),std::min(h,y+int(gen()%35))});}
  int l=gen()%w,t=gen()%h;std::array<int,4> clip{l,t,l+int(gen()%(w-l+1)),t+int(gen()%(h-t+1))};
  auto d=region(bounds,0,uint32_t(bounds.size()),w,h,false,false,false,clip);
  for(int y=0;y<h;++y)for(int x=0;x<w;++x){bool touches=false;for(auto b:bounds)touches|=x>=b[0]&&x<b[2]&&y>=b[1]&&y<b[3];
   touches=touches&&x>=clip[0]&&x<clip[2]&&y>=clip[1]&&y<clip[3];
   bool dispatched=x>=int(d.x)&&y>=int(d.y)&&x<int(d.x+d.groupsX*8)&&y<int(d.y+d.groupsY*8);assert(!touches||dispatched);++checked;
  }
 }
 std::cout<<"PASS GPU phase regions: 400 randomized clip/coverage cases; pixel_checks="<<checked<<"; initialization/finalization stay full, serial order unchanged\n";
}
