#include "../../shared/source/direct_patch_policy.h"
#include "../../shared/source/patch_context.h"
#include "../../shared/source/feature_availability.h"
#include "../source/hook_registry.h"
#include "../source/tech_enhancement.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>
#include <algorithm>
int main(){
 using namespace assist::hookregistry;
 for(const auto&s:directpatch::Sites){
  Registry<> r;assert(knownOriginal(s.address,s.before,s.size));
  assert(directpatch::installed(s.address,s.after,s.size));
  auto read=[&](uint32_t at,void*p,size_t n){if(at!=s.address+s.size||n!=12)return false;memcpy(p,s.context,12);return true;};
  assert(patchcontext::compatible(s.address,read));
  assert(!patchcontext::compatible(s.address,[](uint32_t,void*p,size_t n){memset(p,0,n);return true;}));
  assert(r.proposal(s.address,s.before,s.after,s.size)==Transition::Install);
  r.commit(s.address,s.before,s.after,s.size,Transition::Install);
  assert(r.proposal(s.address,s.after,s.before,s.size)==Transition::Restore);
  r.commit(s.address,s.after,s.before,s.size,Transition::Restore);assert(!r.active(s.address));
  uint8_t foreign[6];memcpy(foreign,s.after,6);foreign[0]^=1;assert(!directpatch::installed(s.address,foreign,s.size));
  r.markConflict(s.address);assert(r.proposal(s.address,s.before,s.after,s.size)==Transition::Deny);
 }
 assert(!experimentalpatch::size(0x6378E0));assert(!directpatch::original(0x577B3A,(const uint8_t*)"\x75\x39",2));
 std::map<uint32_t,uint8_t> mem;unsigned writes=0;
 auto read=[&](uint32_t a,void*p,size_t n){auto*b=(uint8_t*)p;for(size_t i=0;i<n;++i){auto it=mem.find(a+i);if(it==mem.end())return false;b[i]=it->second;}return true;};
 auto write=[&](uint32_t a,const void*p,size_t n){++writes;auto*b=(const uint8_t*)p;for(size_t i=0;i<n;++i)mem[a+i]=b[i];return true;};
 auto word=[&](uint32_t a,int v){write(a,&v,4);};
 const auto&rule=techenhancement::Rules[1];const uint32_t obj=0x20000000,items=0x21000000;
 auto setup=[&](){mem.clear();word(rule.array+4,items);word(rule.array+16,1);word(items,obj);word(obj,rule.vtable);char id[24]="COW";write(obj+0x24,id,24);word(obj+0x634,-1);writes=0;return techenhancement::Record{obj,items,0,-1,false,false};};
 auto r=setup();assert(techenhancement::apply(rule,r,read,write));int level;read(obj+0x634,&level,4);assert(level==1&&writes==1);
 assert(techenhancement::apply(rule,r,read,write)&&writes==1);assert(techenhancement::restore(rule,r,read,write));read(obj+0x634,&level,4);assert(level==-1);
 r=setup();assert(techenhancement::apply(rule,r,read,write));word(obj+0x634,7);unsigned before=writes;assert(!techenhancement::restore(rule,r,read,write)&&writes==before);read(obj+0x634,&level,4);assert(level==7);
 r=setup();assert(techenhancement::apply(rule,r,read,write));word(items,obj+0x2000);before=writes;assert(!techenhancement::restore(rule,r,read,write)&&writes==before);
 r=setup();word(obj+0x634,1);before=writes;assert(techenhancement::apply(rule,r,read,write)&&!r.owned&&writes==before);
 r=setup();word(obj,0);before=writes;assert(!techenhancement::apply(rule,r,read,write)&&writes==before);
 assert(techenhancement::Count==21&&techenhancement::eligible(techenhancement::Rules[9],11));
 assert(!techenhancement::eligible(rule,-255)&&!techenhancement::eligible(rule,4));
 for(int mode=-1;mode<=4;++mode)assert(featureavailability::refreshMode(mode)==0);
 for(int percent:{50,75,100,125,150})assert(featureavailability::zoomPercent(percent)==100);
 puts("PASS: seven exact direct sites/context/ownership/restore; enhancement full-word write, conflicts and replaced objects; retired display modes");
}
