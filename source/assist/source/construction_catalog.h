#pragma once
#include "construction_policy.h"
#include "../../unit_monitor/source/owned_counter.h"
#include <cstring>
namespace assist { namespace construction {
// Read-only YRpp layout. Keep failures distinct from empty prerequisite lists.
template<class Read> class Catalogue {
 Read read;uint32_t house=0,types=0;int count=0,parent=-1,country=-1,tech=-1;
 std::vector<int> owned,predicted,limits;std::vector<int> groups[6];
 template<class T>T get(uint32_t at,T fallback={})const{T v{};return read(at,&v,sizeof v)?v:fallback;}
 bool list(uint32_t at,std::vector<int>&out)const{
  uint32_t p=get<uint32_t>(at+4);int n=get<int>(at+16,-1),cap=get<int>(at+8,-1);
  if(n<0||n>256||cap<n||cap>65536||(n&&p<0x10000))return false;
  out.assign(size_t(n),0);return !n||read(p,out.data(),size_t(n)*4);
 }
 static bool sameID(const char*a,const char*b){for(int i=0;i<25;++i){unsigned x=(unsigned char)a[i],y=(unsigned char)b[i];if(x>='a'&&x<='z')x-=32;if(y>='a'&&y<='z')y-=32;if(x!=y)return false;if(!x)return true;}return false;}
public:
 explicit Catalogue(Read r):read(r){}
 bool load(uint32_t h,const std::vector<int>&future){
  house=h;uint32_t c=get<uint32_t>(h+0x34),rules=get<uint32_t>(0x8871E0);
  country=get<int>(c+0xB8,-1);parent=get<int>(c+0xB4,-1);tech=get<int>(h+0x1D4,-1);
  // Spawned/player countries can have a different ArrayIndex2 than Owner's
  // parent country. Resolve ParentCountry by ID without calling game code.
  char name[25]{};if(!read(c+0x98,name,25))return false;name[24]=0;
  if(name[0]){parent=-1;uint32_t a=get<uint32_t>(0xA83C98+4);int n=get<int>(0xA83C98+16,-1);
   if(n<0||n>256||a<0x10000)return false;
   for(int i=0;i<n;++i){uint32_t t=get<uint32_t>(a+i*4);char id[25]{};if(t>=0x10000&&read(t+0x24,id,25)&&sameID(name,id)){parent=get<int>(t+0xB4,-1);break;}}
  }
  types=get<uint32_t>(0xA83C68+4);count=get<int>(0xA83C68+16,-1);
  if(country<0||country>=32||parent<0||parent>=32||tech<0||count<=0||count>4096||types<0x10000||rules<0x10000)return false;
  owned.assign(size_t(count),0);limits=owned;int errors=0;
  um::readCounterPrefix(read,h+0x5550,count,owned.data(),errors);
  um::readCounterPrefix(read,h+0x5500,count,limits.data(),errors);if(errors)return false;
  for(int g=0;g<6;++g)if(!list(rules+0x358+g*28,groups[g]))return false;
  predicted=owned;for(int i:future)if(i>=0&&i<count)++predicted[i];return true;
 }
 uint32_t type(int index)const{return index>=0&&index<count?get<uint32_t>(types+index*4):0;}
 bool possible(uint32_t t,int kind=7,bool naval=false,bool forecast=true,bool technology=false)const{
  if(t<0x10000)return false;
  const auto&counts=forecast?predicted:owned;
  if(!technology){
  int level=get<int>(t+0x634,-1);if(level<0||level>tech)return false;
  if(!countryAllowed(get<uint32_t>(t+0x6CC),get<uint32_t>(t+0xDA0),get<uint32_t>(t+0xDA4),parent,country))return false;
  // Third/Soviet/Allied stolen-tech flags are D9B..D9D, not D9D..D9F.
  for(int i=0;i<3;++i)if(get<uint8_t>(t+0xD9B+i,1)&&!get<uint8_t>(house+0x2BC+i))return false;
  std::vector<int> needs,overrides;if(!list(t+0x638,needs)||!list(t+0x654,overrides))return false;
  if(!available(needs,overrides,counts,groups))return false;
  }
  if(kind!=7){bool producer=false;for(int n=0;n<count;++n)if(counts[n]>0){uint32_t b=type(n);
   if(b&&get<int>(b+0xEB8,-1)==kind&&bool(get<uint8_t>(b+0xCCE))==naval){producer=true;break;}}
   if(!producer)return false;
  }
  // This is a preview, never authorization. Runtime rechecks the actual cameo
  // and original CanBuild before constructing a native production event.
  return true;
 }
 bool preview(int index)const{uint32_t t=type(index);if(!possible(t)||possible(t,7,false,false))return false;
  int limit=get<int>(t+0x3B8);return limit<=0||int64_t(limits[index])+predicted[index]-owned[index]<limit;
 }
 bool append(int index,bool technology=false){uint32_t t=type(index);if(!possible(t,7,false,true,technology))return false;++predicted[index];return true;}
};
}}
