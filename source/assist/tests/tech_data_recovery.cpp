#include <cstdint>
#include <cstring>
#include <map>
#include <cstdio>
#include "../source/tech_data_policy.h"
using namespace assist;
constexpr uint32_t Type=0x20000,Array=0xA83CE0,Items=0x30000;
struct Memory {
 std::map<uint32_t,uint8_t> bytes;uint32_t unreadable=0,failWrite=0;bool after=false;unsigned writes=0;
 template<class T>void seed(uint32_t at,T value){auto*p=(const uint8_t*)&value;for(unsigned n=0;n<sizeof value;++n)bytes[at+n]=p[n];}
 bool read(uint32_t at,void*out,size_t n){if(unreadable>=at&&unreadable<uint64_t(at)+n)return false;auto*p=(uint8_t*)out;for(unsigned i=0;i<n;++i){auto x=bytes.find(at+i);if(x==bytes.end())return false;p[i]=x->second;}return true;}
 bool write(uint32_t at,const void*p,size_t n){++writes;if(at==failWrite&&!after)return false;auto*b=(const uint8_t*)p;for(unsigned i=0;i<n;++i)bytes[at+i]=b[i];return at!=failWrite;}
 uint32_t field(unsigned i){uint32_t n=0;read(Type+techdata::Fields[i].offset,&n,techdata::Fields[i].size);return n;}
 void setup(){*this={};seed<uint32_t>(Array+4,Items);seed<int>(Array+16,1);seed<uint32_t>(Items,Type);seed<uint32_t>(Type,techdata::vtable(40));seed<uint32_t>(Type+0x10,99);char id[25]="TESTTYPE";for(unsigned i=0;i<25;++i)seed<uint8_t>(Type+0x24+i,uint8_t(id[i]));
  for(unsigned i=0;i<techdata::FieldCount;++i){auto&f=techdata::Fields[i];if(f.size==1)seed<uint8_t>(Type+f.offset,1);else seed<uint32_t>(Type+f.offset,i==1?2:8);}
  seed<uint32_t>(Type+0x63C,0x40000);seed<int>(Type+0x640,4);seed<int>(Type+0x3B8,3);
 }
};
int main(){unsigned checks=0,failed=0;auto check=[&](bool ok,const char*s){++checks;if(!ok){++failed;std::fprintf(stderr,"FAIL %s\n",s);}};
 Memory m;techdata::Record r;auto read=[&](uint32_t a,void*p,size_t n){return m.read(a,p,n);};auto write=[&](uint32_t a,const void*p,size_t n){return m.write(a,p,n);};
 auto begin=[&](){m.setup();r={};check(techdata::capture(r,Type,40,0,read),"capture exact Type/UID/catalogue");};
 auto original=[&](){for(unsigned i=0;i<techdata::FieldCount;++i)if(m.field(i)!=(i==1?2:techdata::Fields[i].size==1?1:8))return false;return true;};
 begin();const auto initial=m.bytes;check(techdata::apply(r,read,write)&&r.owned==511,"all nine fields owned before writes");
 for(unsigned i=0;i<techdata::FieldCount;++i)check(m.field(i)==techdata::Fields[i].value,"exact desired field value/width");
 unsigned prior=m.writes;check(techdata::apply(r,read,write)&&m.writes==prior,"steady state performs no repeat writes");
 check(techdata::restore(r,techdata::State::Current,read,write)&&m.bytes==initial,"restore exactly original data including untouched limits/vector storage");
 for(unsigned i=0;i<techdata::FieldCount;++i)for(bool after:{false,true}){begin();m.failWrite=Type+techdata::Fields[i].offset;m.after=after;
  check(!techdata::apply(r,read,write),"pre/post write failure rejects application");check(r.owned&(1u<<i),"failed write retains responsibility");m.failWrite=0;
  check(techdata::restore(r,techdata::State::Current,read,write)&&original(),"partial rollback restores every prior field");}
 for(unsigned i=0;i<techdata::FieldCount;++i){begin();m.unreadable=Type+techdata::Fields[i].offset;
  check(!techdata::apply(r,read,write)&&m.writes==0&&!r.owned,"all fields preflight before first mutation");m.unreadable=0;check(techdata::apply(r,read,write),"read retry can apply");
  m.unreadable=Type+techdata::Fields[i].offset;check(!techdata::restore(r,techdata::State::Current,read,write)&&(r.owned&(1u<<i)),"restore read failure retains exact responsibility");m.unreadable=0;
  check(techdata::restore(r,techdata::State::Current,read,write)&&original(),"restore read recovery");}
 begin();techdata::apply(r,read,write);m.seed<uint32_t>(Type+0xDA4,7);check(!techdata::restore(r,techdata::State::Current,read,write)&&r.owned==(1u<<4)&&m.field(4)==7,"foreign field left intact while independent fields restore");
 m.seed<uint32_t>(Type+0xDA4,8);check(techdata::restore(r,techdata::State::Current,read,write)&&!r.owned,"already original foreign recovery clears journal idempotently");
 begin();techdata::apply(r,read,write);prior=m.writes;m.seed<uint32_t>(Type+0x10,100);check(techdata::restore(r,techdata::State::Current,read,write)&&m.writes==prior&&!r.owned,"same pointer different UID never restored through");
 begin();techdata::apply(r,read,write);prior=m.writes;check(!techdata::restore(r,techdata::State::Unavailable,read,write)&&m.writes==prior&&r.owned,"world short read retains journal");
 check(techdata::restore(r,techdata::State::Current,read,write)&&original(),"same world read recovery restores");
 begin();techdata::apply(r,read,write);prior=m.writes;check(techdata::restore(r,techdata::State::Retired,read,write)&&m.writes==prior&&!r.owned,"actual retired world never writes obsolete identity");
 begin();techdata::apply(r,read,write);m.seed<uint32_t>(Array+4,0x31000);m.seed<uint32_t>(0x31000,Type);check(techdata::restore(r,techdata::State::Current,read,write)&&original(),"array buffer reallocation retains same Type identity");
 m.setup();r={};m.seed<int>(Type+0x648,5);check(!techdata::capture(r,Type,40,0,read)&&!r.captured&&m.writes==0,"invalid prerequisite count cannot be normalized");
 m.setup();r={};m.seed<uint32_t>(Type,0x12345678);check(!techdata::capture(r,Type,40,0,read),"unexpected vtable rejected before capture");
 std::printf("technology rule-data recovery checks=%u failures=%u; synthetic memory, no game/netplay\n",checks,failed);return failed?1:0;
}
