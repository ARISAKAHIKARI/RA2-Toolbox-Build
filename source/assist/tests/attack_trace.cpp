#include "../../external_reader/source/attack_trace.h"
#include <cassert>
#include <iostream>
int main(){
 std::vector<uint8_t> m(0x1000000);auto put=[&](uint32_t p,auto v){memcpy(m.data()+p,&v,sizeof v);};
 auto read=[&](uint32_t p,void*out,size_t n){if(uint64_t(p)+n>m.size())return false;memcpy(out,m.data()+p,n);return true;};
 constexpr uint32_t p=0x10000,t=0x20000,w=0x30000,wh=0x40000,house=0x50000,list=0x60000;
 put(0xA83D4C,house);put(0xA8ED84,1u);put(p,0x7F5C70u);put(p+0x6C,100);put(p+0x74,uint8_t(1));put(p+0x90,uint8_t(1));put(p+0x21C,house);put(p+0x6C4,t);put(p+0x10,42u);
 put(0xA8ECB8+4,list);put(0xA8ECB8+8,1);put(0xA8ECB8+16,1);put(list,p);memcpy(m.data()+t+0x24,"MAG",4);
 put(t+0x898,w);put(w+0x15C,uint8_t(1));put(w+0xAC,wh);put(wh+0x15B,uint8_t(1));put(w+0xB4,3072);
 external::attacktrace::Recorder r;std::vector<std::string> lines;auto emit=[&](const std::string&s){lines.push_back(s);};
 auto before=m;r.sample(read,emit,1000,9,1,"flags=0 global=0 drop=0 toolEnabled=0");assert(m==before&&lines.size()==1);assert(lines.back().find("unit=42 type=MAG")!=std::string::npos);
 r.sample(read,emit,1100,9,1,"");assert(lines.size()==1);r.sample(read,emit,1500,9,1,"");assert(lines.size()==1);
 // First shot leaves a beam and target; Stop changes mission/destination;
 // observation continues with all switches disabled and performs no repair.
 put(p+0x2AC,0x70000u);put(p+0x2B4,0x70000u);put(p+0xAC,1);before=m;r.sample(read,emit,1750,9,1,"flags=0");assert(lines.size()==2&&m==before);
 put(p+0xAC,5);put(p+0x5A4,0x80000u);r.sample(read,emit,2000,9,1,"flags=0");assert(lines.size()==3);
 put(p+0x9C,256);r.sample(read,emit,2250,9,1,"");assert(lines.size()==3);r.sample(read,emit,3000,9,1,"");assert(lines.size()==4);
 r.sample(read,emit,8000,9,1,"");assert(lines.size()==5);r.sample(read,emit,8250,10,1,"");assert(lines.size()==6);
 // Other vehicle classes/weapon types are not classified as magnetrons.
 put(w+0x15C,uint8_t(0));r.sample(read,emit,9000,10,1,"");assert(lines.size()==6);
 std::cout<<"PASS read-only attack-state capture, disabled features, discrete changes, throttling and process reset\n";
}
