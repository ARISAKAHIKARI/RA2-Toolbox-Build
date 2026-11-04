#include "../../external_reader/source/particle_visuals.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main(){
 std::vector<uint8_t>memory(0xB10000);auto put=[&](uint32_t p,auto value){std::memcpy(memory.data()+p,&value,sizeof value);};auto read=[&](uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size())return false;std::memcpy(out,memory.data()+p,n);return true;};
 constexpr uint32_t object=0x10000,type=0x20000,items=0x30000;put(object,external::particles::Vtable);put(object+0x10,17u);put(object+0xAC,type);put(object+0x90,uint8_t(1));put(object+0x9C,30*256);put(object+0xA0,32*256);put(object+0x12E,int8_t(5));put(type+0x314,0);
 external::particles::State s;assert(external::particles::capture(read,object,s)&&s.frame==5&&s.id==17);
 for(int mode=0;mode<2;++mode)for(int value=-128;value<128;++value){put(type+0x314,mode);put(object+0x12E,int8_t(value));assert(external::particles::capture(read,object,s)==(value>=0));if(value>=0)assert(s.frame==value);}
 put(object+0x12E,int8_t(5));for(int detail=0;detail<4;++detail)for(int alpha=0;alpha<256;++alpha){put(0xA8EB78,detail);put(object+0x12F,uint8_t(alpha));assert(external::particles::capture(read,object,s));assert(s.blend==(detail==2?(alpha==25?1:alpha==50?2:alpha>=75&&alpha<128?3:0):0));}
 for(int mode:{2,5,-1}){put(type+0x314,mode);assert(!external::particles::capture(read,object,s));} // unsupported families cannot masquerade as generic SHP
 for(int mode:{3,4}){put(type+0x314,mode);assert(external::particles::capture(read,object,s)&&s.point&&s.frame==0);}put(type+0x314,2);put(object+0x124,0x35000u);put(0x35000+0xF4,uint8_t(128));put(type+0x309,uint8_t(20));assert(external::particles::capture(read,object,s)&&s.frame==45&&!s.point);
 put(type+0x314,0);put(object+0x81,uint8_t(1));assert(!external::particles::capture(read,object,s));put(object+0x81,uint8_t(0));
 put(external::particles::Array+4,items);put(external::particles::Array+8,600);put(external::particles::Array+16,600);
 for(int i=0;i<600;++i){uint32_t p=0x40000+i*0x200;std::memcpy(memory.data()+p,memory.data()+object,0x138);put(p+0x10,uint32_t(i+1));put(items+i*4,p);}
 assert(external::particles::all(read).size()==600);
 std::cout<<"PASS particle SHP identity, 512 signed native frames, 1024 detail/opacity combinations, 600-particle snapshot; directional fire plus native pixel families\n";
}
