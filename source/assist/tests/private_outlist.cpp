#ifdef NDEBUG
#error Assertions must remain enabled
#endif
#include <cassert>
#include <initializer_list>
#include <cstdio>
#include <cstring>
#include "../source/private_outlist.h"
#include "../source/private_outlist_sites.h"
#include "../../shared/source/hardware_slot_budget.h"
using namespace assist;
static privateout::Queue queue;
static vanilla::Packet event(unsigned kind,unsigned house,unsigned id=0x1234){
 vanilla::Packet e{};e.kind=kind;e.house=char(house);e.frame=1230;
 for(unsigned i=0;i<sizeof e.payload;++i)e.payload[i]=uint8_t(i+1);
 if(kind==4){const uint8_t p[]={0x34,0x12,0,0,0x34,2,0,0x89,0x67,0,0,0x34,0x78,0x56,0,0,11,0x76,0x98,0,0,11,0};memcpy(e.payload,p,sizeof p);memcpy(e.payload,&id,4);}
 if(kind==5){const uint8_t p[]={0x34,0x12,0,0,0x34,2,0x89,0x67,0,0,0x34,0x78,0x56,0,0,11,3,0,0,0,4,0,0,0};memcpy(e.payload,p,sizeof p);}
 vanilla::Packet clean{};assert(privateout::canonical(clean,e));return clean;
}
int main(int argc,char**argv){
 privateout::Pending p{};for(unsigned i=0;i<privateout::Capacity;++i){p.stamp.epoch=i;assert(queue.push(p));}assert(!queue.push(p));
 for(unsigned i=0;i<3000;++i){assert(queue.at(0).stamp.epoch==i);assert(queue.pop());}
 for(unsigned i=0;i<3000;++i){p.stamp.epoch=4096+i;assert(queue.push(p));}
 for(unsigned i=3000;i<7096;++i){assert(queue.at(0).stamp.epoch==i);assert(queue.pop());}assert(!queue.pop());
 assert(slotbudget::allows(slotbudget::pack(4,0,0,0),slotbudget::PrivateOut|slotbudget::Control));
 assert(!slotbudget::allows(slotbudget::pack(2,2,0,slotbudget::Control|slotbudget::Map),slotbudget::PrivateOut));
 assert(slotbudget::tags(slotbudget::pack(0,4,0,slotbudget::PrivateOut|slotbudget::Control))==(slotbudget::PrivateOut|slotbudget::Control));
 uint32_t prior=0;for(const auto&s:privateout::CaptureSites){assert(s.caller>prior);prior=s.caller;assert(privateout::captureSite(s.caller)==&s);assert(!privateout::captureSite(s.caller+1));}
 privateout::Wire w;vanilla::Packet events[24]{};
 for(unsigned kind=0;kind<256;++kind)if(privateout::payloadBytes(kind)>=0){
  for(int protocol=0;protocol<3;++protocol)for(unsigned house:{0u,1u,7u,31u}){
   events[0]=event(kind,house);assert(privateout::encode(w,events,1,protocol,1230,15,5,0x12345678,65535));
   assert(w.count==1&&w.local[0].frame==1245&&w.local[0].executed==0);
   if(argc>1){printf("%u %u %u ",kind,protocol,house);for(unsigned i=0;i<w.size;++i)printf("%02x",w.bytes[i]);printf(" ");for(auto c:reinterpret_cast<unsigned char(&)[111]>(w.local[0]))printf("%02x",c);puts("");}
  }
 }
 for(unsigned i=0;i<24;++i)events[i]=event(i%3==0?4:i%3==1?7:0x2a,0,0x1234+i);
 for(int protocol=0;protocol<3;++protocol){assert(privateout::encode(w,events,24,protocol,1230,15,5,0x12345678,65530));if(argc>1){printf("256 %u 0 ",protocol);for(unsigned i=0;i<w.size;++i)printf("%02x",w.bytes[i]);printf(" ");for(auto&e:w.local)for(auto c:reinterpret_cast<unsigned char(&)[111]>(e))printf("%02x",c);puts("");}}
 assert(!privateout::encode(w,events,25,1,1230,15,5,0,0));assert(!privateout::encode(w,events,1,2,1231,15,5,0,0));
 assert(!privateout::encode(w,events,1,2,1230,13,5,0,0));assert(!privateout::encode(w,events,1,0,INT32_MAX,15,5,0,0));
 assert(!privateout::encode(w,events,0,0,1230,15,5,0,0));events[0].kind=0x1c;assert(!privateout::encode(w,events,1,1,1230,15,5,0,0));
 if(argc==1)puts("PASS queue wrap/full/order, codec bounds, command allowlist, 42 return sites and shared three-slot budget");
}
