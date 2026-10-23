#include "../../external_reader/source/visual_pose.h"
#include <cassert>
#include <iostream>
int main(){
 std::vector<uint8_t> m(0x1000000);auto put=[&](uint32_t p,auto v){memcpy(m.data()+p,&v,sizeof v);};auto read=[&](uint32_t p,void*out,size_t n){if(uint64_t(p)+n>m.size())return false;memcpy(out,m.data()+p,n);return true;};external::GameView g(read);
 constexpr uint32_t p=0x10000,t=0x20000,f=p+0x388;put(f,uint16_t(16384));put(f+4,uint16_t(0));put(f+8,100);put(f+16,16);put(f+20,int16_t(1024));
 assert(external::visual::facing(g,f,100)==0);assert(external::visual::facing(g,f,108)==8192);assert(external::visual::facing(g,f,116)==16384);
 put(f,uint16_t(1024));put(f+4,uint16_t(64512));put(f+16,2);assert(external::visual::facing(g,f,101)==0);
 put(f,uint16_t(64512));put(f+4,uint16_t(1024));assert(external::visual::facing(g,f,101)==0);
 put(f,uint16_t(0));put(f+8,-1);put(f+16,0);put(t+0xCA1,uint8_t(1));put(p+0x3A0,uint16_t(16384));put(t+0xB0,0x30000u);
 auto pose=external::visual::pose(g,p,t,2,100);assert(pose.body==0&&pose.turret==8);
 put(f,uint16_t(32768));pose=external::visual::pose(g,p,t,2,100);assert(pose.body==16&&pose.turret==8);
 // Infantry sequence frames include animation progress and its actual facing.
 put(t+0xE3C,0x30000u);put(p+0x6C4,2);put(0x30000+72,80);put(0x30000+76,6);put(0x30000+80,6);put(p+0xF8,8);put(0x7EAEFC+16*4,5);
 pose=external::visual::pose(g,p,t,1,100);assert(pose.frame==112);put(p+0x6C4,42);assert(external::visual::pose(g,p,t,1,100).frame==-1);
 // Preview uses the Ready sequence and each requested heading, even when the
 // live actor is running or firing. The lookup intentionally is not linear.
 put(0x30000,16);put(0x30004,1);put(0x30008,1);
 int lookup[8]={0,7,6,5,4,3,2,1};
 for(int i=0;i<32;++i)put(0x7EAEFC+i*4,lookup[((i+2)/4)&7]);
 for(int heading=0;heading<32;heading+=4){int body=(8-heading)&31;assert(external::visual::previewFrame(g,t,1,heading)==16+lookup[((body+2)/4)&7]);}
 // SHP vehicle standing frames use the native +1 facing convention.
 put(t+0xB0,0u);put(t+0xE3C,8);put(p+0x6C0,-1);put(t+0xE1C,2);put(t+0xE28,10);pose=external::visual::pose(g,p,t,2,100);assert(pose.frame==20);
 std::cout<<"PASS current facing interpolation, wraparound, independent turret, infantry sequence and SHP vehicle directions\n";
}
