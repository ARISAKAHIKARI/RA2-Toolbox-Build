#include "../source/shroud_material.h"
#include "../source/shroud_policy.h"
#include <cassert>
#include <iostream>
int main(){using assist::shroudmaterials::Controller;
 std::vector<uint8_t>memory(0xA00000);unsigned writes=0;int failAt=-1;bool partial=false;uint32_t unreadableHeader=0;
 auto put=[&](uint32_t at,auto value){std::memcpy(memory.data()+at,&value,sizeof value);};
 auto read=[&](uint32_t at,void*out,size_t n){if(at==unreadableHeader||uint64_t(at)+n>memory.size())return false;std::memcpy(out,memory.data()+at,n);return true;};
 auto write=[&](uint32_t at,const void*in,size_t n){assert(at>=0x10000&&at<0x100000);++writes;if(failAt==int(writes)){if(partial)std::memcpy(memory.data()+at,in,n/2);return false;}std::memcpy(memory.data()+at,in,n);return true;};
 auto shape=[&](uint32_t at){put(at,uint16_t(0));put(at+2,uint16_t(60));put(at+4,uint16_t(30));put(at+6,uint16_t(47));
  for(unsigned f=0;f<47;++f){uint32_t h=at+8+f*24;put(h,uint16_t(0));put(h+2,uint16_t(0));put(h+4,uint16_t(60));put(h+6,uint16_t(30));put(h+8,0u);put(h+20,8+47*24+f*1800);
   for(unsigned n=0;n<1800;++n)memory[at+8+47*24+f*1800+n]=n%17==0?254:uint8_t(n%128);}
 };
 constexpr uint32_t a=0x10000,b=0x30000,ref=0x60000;shape(a);shape(b);put(0x89E790,a);put(0x89E794,a);put(0x89E760,b);
 auto original=memory;Controller c;assert(c.update(1,10,read,write)==1&&c.changed()&&writes==94);
 for(uint32_t at:{a,b})for(unsigned n=0;n<47*1800;++n){auto v=original[at+8+47*24+n];assert(memory[at+8+47*24+n]==(v<=127?127:v));}
 auto stable=memory;unsigned before=writes;for(unsigned t=11;t<1011;++t)assert(c.update(1,t,read,write)==1&&!c.changed());assert(writes==before&&memory==stable);
 assert(c.update(2,1020,read,write)==1);for(uint32_t at:{a,b})for(unsigned n=0;n<47*1800;++n){auto v=original[at+8+47*24+n];assert(memory[at+8+47*24+n]==(v<=127?(v+128)/2:v));}
 assert(c.restore(read,write)&&!c.active()&&memory==original);
 // Temporary header failure must not discard the only saved original alpha.
 // Other resources can restore while the unavailable allocation remains owned.
 assert(c.update(1,1500,read,write)==1);unreadableHeader=a;before=writes;
 assert(!c.restore(read,write)&&c.active()&&c.status()==-2&&writes==before+47);
 assert(memory[a+8+47*24+1]==127);
 assert(std::equal(original.begin()+b,original.begin()+b+8+47*24+47*1800,memory.begin()+b));
 unreadableHeader=0;assert(c.restore(read,write)&&!c.active()&&memory==original);
 // A failed write, including a partial write, restores the entire applied group.
 failAt=int(writes)+11;partial=true;assert(c.update(1,2000,read,write)==-2&&!c.active()&&memory==original);failAt=-1;partial=false;
 // Lazy refs keep their own allocation and can change the loaded pointer
 // without changing the outer reference address.
 put(0x89E790,ref);put(0x89E794,0u);put(0x89E760,0u);put(ref,uint16_t(0xffff));put(ref+12,0u);put(ref+16,uint8_t(0));
 assert(c.update(1,2100,read,write)==2&&!c.active());put(ref+12,a);put(ref+16,uint8_t(1));assert(c.update(1,2101,read,write)==1);
 put(ref+12,b);assert(c.update(1,2102,read,write)==1);assert(std::equal(original.begin()+a,original.begin()+a+8+47*24+47*1800,memory.begin()+a));assert(c.restore(read,write));
 // An unsupported compressed frame changes nothing; never corrupt RLE.
 put(a+8+8,2u);put(0x89E790,a);before=writes;assert(c.update(1,2200,read,write)==-2&&writes==before);put(a+8+8,0u);
 // Different frame ranges may share an exact plane, but partial overlap is
 // not a valid independent alpha plane and must never be modified.
 put(a+8+24+20,uint32_t(8+47*24+1));before=writes;assert(c.update(1,2201,read,write)==-2&&writes==before);put(a+8+24+20,uint32_t(8+47*24+1800));
 put(a+8+20,uint32_t(b-a));put(0x89E760,b);before=writes;assert(c.update(1,2202,read,write)==-2&&writes==before);put(a+8+20,uint32_t(8+47*24));put(0x89E760,0u);
 // Foreign pixels are preserved and a conflict remains visible to callers.
 assert(c.update(1,2300,read,write)==1);uint32_t pixel=a+8+47*24;memory[pixel]=200;assert(!c.restore(read,write)&&memory[pixel]==200);memory[pixel]=254;assert(c.restore(read,write));
 // Freed/reused source identity is never overwritten during shutdown.
 assert(c.update(1,2400,read,write)==1);put(a+2,uint16_t(55));before=writes;assert(c.restore(read,write)&&writes==before);
 static_assert(assist::shroudpolicy::runtimeMode(5)==0&&assist::shroudpolicy::runtimeMode(6)==0,"unusable material prototype is unavailable to UI and saved preferences");
 std::cout<<"PASS 2 full 47-frame alpha banks: clear/half/restore, shared refs, 1000 warm ticks without writes, transient-header recovery, partial rollback, lazy replacement, compressed rejection and foreign/freed-source isolation\n";
}
