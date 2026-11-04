#include "../source/initial_sync_gate.h"
#include "../../shared/source/multiplayer_patch_gate.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>
using namespace assist::initialsync;
struct Memory {
 std::map<uint32_t,uint8_t> bytes;unsigned reads=0;
 void put(uint32_t at,const void*p,size_t n){auto*b=(const uint8_t*)p;for(size_t i=0;i<n;++i)bytes[at+uint32_t(i)]=b[i];}
 template<class T> void put(uint32_t at,T value){put(at,&value,sizeof value);}
 bool read(uint32_t at,void*out,size_t n){++reads;auto*b=(uint8_t*)out;for(size_t i=0;i<n;++i){auto p=bytes.find(at+uint32_t(i));if(p==bytes.end())return false;b[i]=p->second;}return true;}
 void ring(int count,int head=0){Ring r{count,head,(head+count)&int(Mask)};put(assist::eventdelivery::DoListAddress,r);}
 void event(unsigned slot,unsigned house,unsigned frame,unsigned delay,unsigned crc,bool executed=false){
  Header h{};h.bytes[0]=0x1c;h.bytes[1]=executed;h.bytes[2]=uint8_t(house);h.bytes[13]=uint8_t(delay);memcpy(h.bytes+3,&frame,4);memcpy(h.bytes+7,&crc,4);put(Events+slot*EventBytes,h);put(History+((frame-delay)&255)*4,crc);
 }
};
Identity world(unsigned epoch=1){return {epoch,0x20000000,0x20001000,0x20002000,100,3};}
int main(){
 unsigned cases=0;
 auto sample=[](Gate&g,Memory&m,int frame,uint32_t peers=2,Identity id=world(),bool oos=false){return g.update(id,frame,peers,oos,[&](uint32_t at,void*out,size_t n){return m.read(at,out,n);});};
 {
  Gate g;Memory m;m.ring(0);assert(!sample(g,m,0));assert(!sample(g,m,100000));assert(g.state==Waiting);++cases;
 }
 {
  Gate g;Memory m;m.ring(1);sample(g,m,10);m.event(0,1,20,5,0x1234);
  assert(!sample(g,m,16));auto untouched=m.bytes;assert(!sample(g,m,19));assert(m.bytes==untouched);
  m.event(0,1,20,5,0x1234,true);assert(!sample(g,m,20));assert(sample(g,m,21));assert(g.confirmed==2);++cases;
  assert(!sample(g,m,22,2,world(),true)&&g.state==Mismatch);++cases;
 }
 {
  Gate g;Memory m;m.ring(1);sample(g,m,10);m.event(0,1,20,5,0x1234,true);
  assert(!sample(g,m,21));assert(!sample(g,m,22));assert(!g.ready);++cases;
 }
 {
  Gate g;Memory m;m.ring(1);sample(g,m,10);m.event(0,0,20,5,0x1234);sample(g,m,16);m.event(0,0,20,5,0x1234,true);assert(!sample(g,m,21));++cases;
 }
 {
  Gate g;Memory m;m.ring(1);sample(g,m,10);m.event(0,1,20,5,0x1234);sample(g,m,16);
  m.event(0,1,20,5,0x1234,true);m.put(History+15*4,uint32_t(0x4321));auto untouched=m.bytes;
  assert(!sample(g,m,21)&&g.state==Mismatch);assert(m.bytes==untouched);assert(!sample(g,m,22));++cases;
 }
 {
  Gate g;Memory m;m.ring(1);sample(g,m,10);m.event(0,1,20,5,7);sample(g,m,16);
  m.event(0,1,20,5,7,true);assert(!sample(g,m,271));++cases;
 }
 {
  Gate g;Memory m;m.ring(1);sample(g,m,10);m.event(0,1,20,5,7);sample(g,m,16);
  m.event(0,1,20,5,7,true);assert(sample(g,m,21));assert(!sample(g,m,22,2,world(2)));++cases;
  assert(!sample(g,m,0));assert(!g.ready);++cases;
 }
 {
  Gate g;Memory m;m.ring(1);sample(g,m,10);m.event(0,1,20,5,7);sample(g,m,16);
  m.event(0,1,21,5,7,true);assert(!sample(g,m,22));++cases;
 }
 {
  Gate g;Memory m;m.ring(2,16383);sample(g,m,10,6);m.event(16383,1,20,5,7);m.event(0,2,20,5,7);
  assert(!sample(g,m,16,6));m.event(16383,1,20,5,7,true);assert(!sample(g,m,21,6));m.event(0,2,20,5,7,true);assert(sample(g,m,22,6));++cases;
 }
 {
  Gate g;Memory m;m.ring(16384);sample(g,m,0);m.reads=0;assert(!sample(g,m,1));assert(m.reads<=ScanBudget+1);++cases;
  m.ring(0);assert(sample(g,m,2,0));++cases;
  auto id=world();id.mode=5;assert(sample(g,m,0,0,id));id.mode=2;assert(!sample(g,m,0,0,id));++cases;
 }
 {
  using namespace multiplayerpatch;Memory m;
  m.put(0xA8B238,int(3));m.put(0xA8B230,uint32_t(0x20000000));m.put(0xA8022C,uint32_t(0x20001000));m.put(0xA83D4C,uint32_t(0x20002000));
  m.put(0x20002010,uint32_t(100));m.put(0xA8ED84,int(100));m.put(0xA8E9A0,uint8_t(1));m.put(0xA8B8C2,uint8_t(0));
  auto can=[&](){return mayInstall([&](uint32_t at,void*out,size_t n){return m.read(at,out,n);});};
  // No DoList, CRC history or approval token exists in this fixture.
  assert(can());++cases;
  m.put(0xA8ED84,int(0));assert(can());m.put(0xA8B238,int(4));assert(can());++cases;
  m.put(0xA8B8C2,uint8_t(1));assert(!can());m.put(0xA8B8C2,uint8_t(0));
  m.put(0xA8E9A0,uint8_t(0));assert(!can());m.put(0xA8E9A0,uint8_t(1));++cases;
  m.put(0xA83D4C,uint32_t(0));assert(!can());m.put(0xA83D4C,uint32_t(0x20002000));
  m.put(0xA8ED84,int(-1));assert(!can());m.put(0xA8ED84,int(1));++cases;
  m.put(0xA8B238,int(0));assert(can());m.put(0xA8B238,int(5));assert(can());
  m.put(0xA8B238,int(2));assert(!can());++cases;
 }
 printf("PASS: %u retired observer and current local-only patch-readiness cases; no game or network execution\n",cases);
}
