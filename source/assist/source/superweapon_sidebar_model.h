#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <functional>
#include <vector>
namespace swsidebar {
struct Options {
 bool enabled=false,detached=false,collapsed=false;
 int x=20,y=150,scale=100,rows=8;
};
using Read=std::function<bool(uint32_t,void*,size_t)>;
struct Entry {
 uint32_t instance=0,type=0,shape=0;int index=0,frames=0,percent=0;
 bool ready=false,suspended=false,draining=false;
 char id[25]{};wchar_t name[41]{};
 bool operator==(const Entry&o)const{return instance==o.instance&&type==o.type&&shape==o.shape&&index==o.index&&frames/15==o.frames/15&&percent==o.percent&&ready==o.ready&&suspended==o.suspended&&draining==o.draining&&!std::strcmp(id,o.id)&&!std::wcscmp(name,o.name);}
};
template<class T>inline bool get(const Read&read,uint32_t at,T&v){return at>=0x10000&&uint64_t(at)+sizeof v<=UINT32_MAX&&read(at,&v,sizeof v);}
// Shared with the existing bridge/tactical_sampler and the fixed upstream
// reference. No layout is inferred from an unadjusted YRpp structure.
inline bool sample(const Read&read,std::vector<Entry>&out,int&screenW,int&screenH,int&mapRight){
 uint32_t house=0,items=0,houseAfter=0;int count=0,capacity=0,frame=0,frameAfter=0,viewX=0,viewW=0;
 if(!get(read,0xA83D4C,house)||house<0x10000||!get(read,0xA8ED84,frame)||
    !get(read,house+0x258,items)||!get(read,house+0x25C,capacity)||!get(read,house+0x264,count)||
    count<0||count>256||capacity<count||capacity>65536||(count&&items<0x10000)||
    !get(read,0x8A00A4,screenW)||!get(read,0x8A00A8,screenH)||screenW<320||screenH<200||screenW>32768||screenH>32768||
    !get(read,0x886FA0,viewX)||!get(read,0xB0CE30,viewW)||viewW<1||viewW>8192||viewX<0||viewX>8192)return false;
 std::vector<uint32_t> pointers(size_t(count),0);if(count&&!read(items,pointers.data(),pointers.size()*4))return false;
 std::vector<Entry> next;next.reserve(count);
 for(int n=0;n<count;++n){uint32_t p=pointers[n],owner=0,t=0;uint8_t present=0,ready=0,suspended=0,drain=0,manualControl=0;
  if(p<0x10000||!get(read,p+0x2C,owner)||owner!=house||!get(read,p+0x6D,present)||!present||!get(read,p+0x28,t)||t<0x10000)continue;
  Entry e;e.instance=p;e.type=t;e.index=n;
  if(!read(t+0x24,e.id,24)||!std::memchr(e.id,0,24)||!e.id[0]||!get(read,t+0xF5,manualControl)||manualControl)continue;
  uint32_t name=0;get(read,t+0x60,name);if(name>=0x10000)read(name,e.name,sizeof e.name-2);e.name[40]=0;
  if(!e.name[0])for(int j=0;j<24&&e.id[j];++j)e.name[j]=wchar_t(uint8_t(e.id[j]));
  int start=-1,left=0,total=-1,drainState=0;
  if(!get(read,p+0x30,start)||!get(read,p+0x38,left)||!get(read,p+0x24,total)||!get(read,p+0x6F,ready)||!get(read,p+0x70,suspended)||!get(read,t+0xE5,drain))continue;
  if(total==-1&&!get(read,t+0xB0,total))continue;
  if(drain&&(!get(read,p+0x7C,drainState)||drainState<0||drainState>2))continue;
  e.frames=int(std::clamp<int64_t>(int64_t(left)-(start<0?0:std::max<int64_t>(0,int64_t(frame)-start)),0,INT32_MAX));
  e.suspended=suspended!=0;e.draining=drain&&drainState==2;e.ready=!e.suspended&&(drain?drainState!=0:ready!=0);
  e.percent=e.ready?100:total>0?int(std::clamp<int64_t>((int64_t(total)-e.frames)*100/total,0,100)):0;
  get(read,t+0xB8,e.shape);next.push_back(e);
 }
 uint32_t itemsAfter=0;int countAfter=0;
 if(!get(read,0xA83D4C,houseAfter)||houseAfter!=house||!get(read,0xA8ED84,frameAfter)||frameAfter<frame||frameAfter-frame>2||
    !get(read,house+0x258,itemsAfter)||itemsAfter!=items||!get(read,house+0x264,countAfter)||countAfter!=count)return false;
 out=std::move(next);mapRight=viewX+viewW;return true;
}

}
