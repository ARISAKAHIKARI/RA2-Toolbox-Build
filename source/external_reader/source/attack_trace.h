#pragma once
#include "game_view.h"
#include <array>
#include <map>
#include <sstream>

namespace external { namespace attacktrace {
// Host-only observation. No virtual calls, events, unit writes or feature gate:
// a failure that survives disabling automation must remain observable.
class Recorder {
 struct Previous {std::array<uint32_t,44> state{};uint32_t at=0,seen=0,id=0;bool valid=false;};
 std::map<uint32_t,Previous> previous;
 uint64_t connection=0;uint32_t match=0,last=0;
public:
 template<class Read,class Emit>void sample(Read read,Emit emit,uint32_t now,uint64_t identity,uint32_t epoch,const std::string&context){
  if(connection!=identity||match!=epoch){previous.clear();last=0;connection=identity;match=epoch;}
  if(last&&uint32_t(now-last)<250)return;last=now;
  GameView<Read> g(read);uint32_t house=g.template get<uint32_t>(0xA83D4C),frame=g.template get<uint32_t>(0xA8ED84);
  int count=0;
  for(uint32_t p:g.vec(0xA8ECB8,1024)){
   if(count>=8)break;
   if(g.kind(p)!=2||!g.live(p)||g.template get<uint32_t>(p+0x21C)!=house)continue;
   uint32_t type=g.type(p,2);if(type<0x10000)continue;
   const bool elite=g.template get<float>(p+0x150)>=2;
   uint32_t weapons[2]{};bool magnetic=false;
   for(int i=0;i<2;++i){weapons[i]=g.template get<uint32_t>(type+0x898+i*28);uint32_t e=elite?g.template get<uint32_t>(type+0xA94+i*28):0;if(e)weapons[i]=e;
    if(weapons[i]>=0x10000){uint32_t wh=g.template get<uint32_t>(weapons[i]+0xAC);magnetic|=g.template get<uint8_t>(weapons[i]+0x15C)&&wh>=0x10000&&g.template get<uint8_t>(wh+0x15B);}}
   if(!magnetic)continue;++count;
   std::array<uint8_t,0x6C8> bytes{};if(!g.bytes(p,bytes.data(),bytes.size()))continue;
   auto word=[&](unsigned at){uint32_t v;std::memcpy(&v,bytes.data()+at,4);return v;};
   uint32_t id=word(0x10),target=word(0x2B4),victim=word(0x2AC),loco=word(0x674);
   auto byteFlags=[&](std::initializer_list<unsigned> offsets){uint32_t v=0;unsigned n=0;for(auto at:offsets)v|=uint32_t(bytes[at]!=0)<<n++;return v;};
   // First 32 values are discrete state; remaining values can change while
   // moving/firing and are throttled to one report per second.
   std::array<uint32_t,44> s={id,type,word(0xAC),word(0xB4),target,word(0x2B8),victim,word(0x2B0),word(0x5A4),word(0x5A8),word(0x5C0),word(0x5C4),word(0x5C8),word(0x5CC),word(0x2E4),loco,g.template get<uint32_t>(loco),word(0x1CC),word(0x1D0),
    byteFlags({0x82,0x1C8,0x279,0x3D5,0x68C,0x68D,0x6AD,0x6AE,0x6B6,0x6B7}),
    victim>=0x10000?g.template get<uint32_t>(victim+0x2B0):0,
    weapons[0],weapons[0]>=0x10000?g.template get<uint32_t>(weapons[0]+0xB8):0,weapons[0]>=0x10000?g.template get<uint32_t>(weapons[0]+0xB4):0,
    weapons[1],weapons[1]>=0x10000?g.template get<uint32_t>(weapons[1]+0xB8):0,weapons[1]>=0x10000?g.template get<uint32_t>(weapons[1]+0xB4):0,
    word(0x598),word(0x5B8),word(0x640),word(0x644),word(0x648),
    word(0x9C),word(0xA0),word(0xA4),target>=0x10000?g.template get<uint32_t>(target+0x9C):0,target>=0x10000?g.template get<uint32_t>(target+0xA0):0,target>=0x10000?g.template get<uint32_t>(target+0xA4):0,
    word(0x2EC),word(0x2F4),word(0x3B8),word(0x5E0),word(0x638),word(0x63C)};
   if(g.template get<uint32_t>(p+0x10)!=id||g.template get<uint32_t>(0xA8ED84)!=frame)continue;
   auto&prev=previous[p];prev.seen=now;
   bool core=!prev.valid||!std::equal(s.begin(),s.begin()+32,prev.state.begin());
   if(!core&&(s==prev.state||uint32_t(now-prev.at)<1000)&&uint32_t(now-prev.at)<5000)continue;
   char name[25]{};g.bytes(type+0x24,name,24);for(char&c:name)if(c&&(c<32||c>126))c='?';name[24]=0;
   std::ostringstream out;out<<"magnetron-state frame="<<frame<<" unit="<<id<<" type="<<name<<" address="<<p<<" "<<context<<" values=";
   for(size_t i=0;i<s.size();++i){if(i)out<<',';out<<s[i];}emit(out.str());prev.state=s;prev.at=now;prev.id=id;prev.valid=true;
  }
  for(auto it=previous.begin();it!=previous.end();)if(uint32_t(now-it->second.seen)>10000)it=previous.erase(it);else ++it;
 }
};
}}
