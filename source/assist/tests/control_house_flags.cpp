#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include "../source/control_mode_policy.h"
#include "../source/control_house_flags.h"
using namespace assist;
struct Memory {
 std::map<uint32_t,uint8_t> bytes;
 uint32_t readFail=0,writeFail=0;bool failAfterWrite=false;unsigned writes=0;
 template<class T>void put(uint32_t at,T value){const auto*p=(const uint8_t*)&value;for(unsigned i=0;i<sizeof value;++i)bytes[at+i]=p[i];}
 template<class T>T get(uint32_t at){T value{};assert(read(at,&value,sizeof value));return value;}
 bool read(uint32_t at,void*out,size_t size){
  if(readFail>=at&&readFail<uint64_t(at)+size)return false;
  auto*p=(uint8_t*)out;for(unsigned i=0;i<size;++i){auto v=bytes.find(at+i);if(v==bytes.end())return false;p[i]=v->second;}return true;
 }
 bool write(uint32_t at,const void*in,size_t size){
  // Retired model fixture writes one flag byte to simulate old responsibility.
  assert(size==1&&(at==0x101ED||at==0x201ED));++writes;
  if(writeFail==at&&!failAfterWrite)return false;
  bytes[at]=*(const uint8_t*)in;return writeFail!=at;
 }
 Memory(){
  put<uint32_t>(0xA8022C,0x30000);put<int>(0xA80238,2);
  put<uint32_t>(0x30000,0x10000);put<uint32_t>(0x30004,0x20000);
  for(unsigned n=0;n<2;++n){const uint32_t p=0x10000+n*0x10000;
   put<uint32_t>(p,0x7E0000);put<uint32_t>(p+0x10,100+n);put<uint32_t>(p+0x34,0x40000+n*0x10000);put<int>(p+0x30,n);
   put<uint8_t>(p+0x1EC,1);put<uint8_t>(p+0x1ED,0);put<uint32_t>(p+0x21C,0xBAD0+n);
  }
  put<uint32_t>(0xA8B230,0x60000);put<int>(0xA8B238,3);put<int>(0xA8ED84,100);put<uint32_t>(0xA83D4C,0x10000);
 }
};
int main(){
 for(uint32_t mode=0;mode<6;++mode){
  assert(controlmode::native(mode)==(mode==1));assert(controlmode::master(mode)==(mode==2));
  assert(controlmode::proxyOrders(mode)==(mode==1));assert(controlmode::sender(mode)==(mode==0||mode==1));
 }
 Memory m;auto read=[&](uint32_t a,void*p,size_t n){return m.read(a,p,n);};
 auto write=[&](uint32_t a,const void*p,size_t n){return m.write(a,p,n);};
 controlflags::Model model;controlflags::Identity identities[2]{};
 for(unsigned i=0;i<2;++i)assert(controlflags::capture(i,identities[i],read));
 auto start=[&](){assert(model.bind(1,read));for(unsigned i=0;i<2;++i)assert(controlflags::apply(model.records[i],identities[i],read,write));};
 start();assert(model.pending());
 for(unsigned i=0;i<2;++i){const uint32_t p=identities[i].pointer;assert(m.get<uint8_t>(p+0x1ED)==1);assert(m.get<uint8_t>(p+0x1EC)==1);assert(m.get<uint32_t>(p+0x21C)==0xBAD0+i);}
 assert(m.get<uint32_t>(0xA83D4C)==0x10000);
 assert(model.restoreAll(1,read,write)&&!model.pending());
 // An originally true flag belongs to someone else and must remain true.
 m.put<uint8_t>(0x101ED,1);assert(model.bind(1,read));unsigned before=m.writes;
 assert(controlflags::apply(model.records[0],identities[0],read,write));assert(!model.pending()&&m.writes==before);
 assert(model.restoreAll(1,read,write)&&m.get<uint8_t>(0x101ED)==1);m.put<uint8_t>(0x101ED,0);
 // Failure after the write retains responsibility and can be restored.
 assert(model.bind(1,read));m.writeFail=0x101ED;m.failAfterWrite=true;
 assert(!controlflags::apply(model.records[0],identities[0],read,write)&&model.pending());
 m.writeFail=0;assert(model.restoreAll(1,read,write)&&m.get<uint8_t>(0x101ED)==0);
 // Failure before the write is idempotently retired without a needless write.
 assert(model.bind(1,read));m.writeFail=0x101ED;m.failAfterWrite=false;
 assert(!controlflags::apply(model.records[0],identities[0],read,write)&&model.pending());
 m.writeFail=0;before=m.writes;assert(model.restoreAll(1,read,write)&&m.writes==before);
 // A transient House/World read failure keeps only the affected responsibility;
 // other live records still restore, and detach must wait for the retry.
 start();m.readFail=0x10010;assert(!model.restoreAll(1,read,write));
 assert(model.records[0].owned&&!model.records[1].owned&&m.get<uint8_t>(0x201ED)==0);
 m.readFail=0;assert(model.restoreAll(1,read,write));
 start();m.readFail=0xA8B230;assert(!model.restoreAll(1,read,write)&&model.pending());
 m.readFail=0;assert(model.restoreAll(1,read,write));
 // Failed restore-after-write must not release the record until revalidated.
 start();m.writeFail=0x101ED;m.failAfterWrite=true;assert(!model.restoreAll(1,read,write));
 assert(model.records[0].owned);m.writeFail=0;before=m.writes;assert(model.restoreAll(1,read,write)&&m.writes==before);
 // Reused pointer/ID, retired world and rewind must never write stale objects.
 start();m.put<uint32_t>(0x10010,999);before=m.writes;assert(model.restoreAll(1,read,write)&&m.writes==before+1);
 assert(m.get<uint8_t>(0x101ED)==1);m.put<uint32_t>(0x10010,100);m.put<uint8_t>(0x101ED,0);
 // A new command epoch alone can mean a local-player change in the SAME
 // world. Restore the still-live Houses before admitting that new context.
 start();before=m.writes;assert(model.restoreAll(2,read,write)&&m.writes==before+2);
 start();m.put<uint32_t>(0xA8B230,0x70000);before=m.writes;
 assert(model.restoreAll(2,read,write)&&m.writes==before);m.put<uint32_t>(0xA8B230,0x60000);
 m.put<uint8_t>(0x101ED,0);m.put<uint8_t>(0x201ED,0);start();m.put<int>(0xA8ED84,99);before=m.writes;
 assert(model.restoreAll(1,read,write)&&m.writes==before);
 // An unknown external value is never overwritten or silently forgotten.
 m.put<int>(0xA8ED84,100);m.put<uint8_t>(0x101ED,0);m.put<uint8_t>(0x201ED,0);start();m.put<uint8_t>(0x101ED,7);
 assert(!model.restoreAll(1,read,write)&&model.pending()&&m.get<uint8_t>(0x101ED)==7);
 m.put<uint8_t>(0x101ED,0);assert(model.restoreAll(1,read,write));
 puts("PASS retired House ownership model: mode admission, saved values, partial write/read/restore recovery, conflict retention, epoch, rewind and pointer reuse; no current activation route");
}
