#include "../../external_reader/source/building_components.h"
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace external;
int main(int argc,char**argv){
 constexpr uint32_t object=0x10000,type=0x20000;std::vector<uint8_t> mem(0x30000);
 auto put=[&](uint32_t p,int v){memcpy(mem.data()+p,&v,4);};
 uint32_t fail=0;auto read=[&](uint32_t p,void*out,size_t n){if(p==fail||uint64_t(p)+n>mem.size())return false;memcpy(out,mem.data()+p,n);return true;};
 auto sample=[&](int count,int health,int strength,int tech,double yellow,double red){put(object+0x694,count);put(object+0x6C,health);put(type+0xA0,strength);put(type+0x634,tech);return buildings::occupiedFrame(read,object,type,yellow,red);};
 if(argc==2&&std::string(argv[1])=="--probe"){
  std::string line;while(std::getline(std::cin,line)){std::istringstream s(line);int count,health,strength,tech;double yellow,red;s>>count>>health>>strength>>tech>>yellow>>red;std::cout<<sample(count,health,strength,tech,yellow,red)<<'\n';}return 0;
 }
 // Civilians use ConditionRed, independently of the normal ConditionYellow.
 assert(sample(0,100,100,-1,.5,.25)==0);assert(sample(1,100,100,-1,.5,.25)==2);
 assert(sample(1,40,100,-1,.5,.25)==2);assert(sample(0,25,100,-1,.5,.25)==1);
 assert(sample(1,25,100,-1,.5,.25)==1);assert(sample(1,26,100,-1,.5,.25)==2);
 assert(sample(1,30,100,-1,.5,.4)==1);assert(sample(1,30,100,-1,.5,.25)==2);
 // Buildable garrison structures retain their fourth, damaged-occupied frame.
 assert(sample(0,50,100,1,.5,.25)==1);assert(sample(1,50,100,1,.5,.25)==3);
 assert(sample(1,51,100,1,.5,.25)==2);assert(sample(1,40,100,0,.5,.25)==2);
 assert(sample(1,0,100,-1,.5,.25)==1);assert(sample(1,0,100,1,.5,.25)==3);
 assert(sample(1,100,0,1,.5,.25)==-1);assert(sample(1,-1,100,1,.5,.25)==-1);
 for(uint32_t p:{object+0x694,object+0x6C,type+0xA0,type+0x634}){fail=p;assert(sample(1,100,100,-1,.5,.25)==-1);}fail=0;
 for(int n=0;n<100;++n){assert(sample(1,100,100,-1,.5,.25)==2);assert(sample(0,100,100,-1,.5,.25)==0);}
 std::cout<<"PASS native occupied-building frames, civilian and buildable damage thresholds, entry/exit, live rules and unreadable-state retention\n";
}
