#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <functional>
#include <vector>
#include "../../shared/source/native_viewport_projection.h"
namespace swsidebar {
struct Options {
 bool enabled=false,detached=false,collapsed=false;
 int x=20,y=150,scale=100,rows=8;
};
using Read=std::function<bool(uint32_t,void*,size_t)>;
struct Entry {
 uint32_t instance=0,type=0,shape=0,house=0;int index=0,frames=0,percent=0;
 bool ready=false,suspended=false,draining=false;
 char id[25]{};wchar_t name[41]{};
 bool operator==(const Entry&o)const{return instance==o.instance&&type==o.type&&shape==o.shape&&house==o.house&&index==o.index&&(int64_t(frames)+14)/15==(int64_t(o.frames)+14)/15&&percent==o.percent&&ready==o.ready&&suspended==o.suspended&&draining==o.draining&&!std::strcmp(id,o.id)&&!std::wcscmp(name,o.name);}
};
inline bool sameSelection(const Entry&a,const Entry&b){return a.instance==b.instance&&a.type==b.type&&a.house==b.house&&a.index==b.index;}
inline bool bytes(const Read&read,uint32_t at,void*out,size_t n){return at>=0x10000&&uint64_t(at)+n<=UINT32_MAX&&read(at,out,n);}
template<class T>inline bool get(const Read&read,uint32_t at,T&v){return at>=0x10000&&uint64_t(at)+sizeof v<=UINT32_MAX&&read(at,&v,sizeof v);}
// Host release check; Runtime separately verifies queued identity and native
// readiness. A displayed index/250ms picture alone cannot authorize a command.
inline bool selectable(const Read&read,const Entry&e){
 uint32_t house=0,items=0,p=0,type=0,owner=0;int count=0,capacity=0;uint8_t present=0,ready=0,suspended=0,manual=0,drain=0;int drainState=0;
 if(e.index<0||e.index>=256||!get(read,0xA83D4C,house)||house!=e.house||
    !get(read,house+0x258,items)||!get(read,house+0x264,count)||count<=e.index||count>256||
    !get(read,house+0x25C,capacity)||capacity<count||capacity>65536||
    uint64_t(items)+uint64_t(e.index)*4>UINT32_MAX||!get(read,items+uint32_t(e.index)*4,p)||p!=e.instance||
    !get(read,p+0x28,type)||type!=e.type||!get(read,p+0x2C,owner)||owner!=house||
    !get(read,p+0x6D,present)||!present||!get(read,p+0x70,suspended)||suspended||
    !get(read,type+0xF5,manual)||manual||!get(read,type+0xE5,drain))return false;
 if(drain){if(!get(read,p+0x7C,drainState)||drainState<1||drainState>2)return false;}
 else if(!get(read,p+0x6F,ready)||!ready)return false;
 uint32_t after=0;return get(read,0xA83D4C,after)&&after==house;
}
// Shared with the existing bridge/tactical_sampler and the fixed upstream
// reference. No layout is inferred from an unadjusted YRpp structure.
inline bool sample(const Read&read,std::vector<Entry>&out,int&screenW,int&screenH,int&mapRight,const nativeviewportprojection::Transform&transform={}){
 if(transform.active&&(!nativeviewportprojection::valid(transform.physical)||!nativeviewportprojection::valid(transform.logical)))return false;
 uint32_t house=0,items=0,houseAfter=0;int count=0,capacity=0,frame=0,frameAfter=0,viewX=0,viewW=0;
 if(!get(read,0xA83D4C,house)||house<0x10000||!get(read,0xA8ED84,frame)||frame<0||
    !get(read,house+0x258,items)||!get(read,house+0x25C,capacity)||!get(read,house+0x264,count)||
    count<0||count>256||capacity<count||capacity>65536||(count&&items<0x10000)||
    !get(read,0x8A00A4,screenW)||!get(read,0x8A00A8,screenH)||screenW<320||screenH<200||screenW>32768||screenH>32768||
    !get(read,0x886FA0,viewX)||!get(read,0xB0CE30,viewW)||viewW<1||viewW>8192||viewX<0||viewX>8192)return false;
 std::vector<uint32_t> pointers(size_t(count),0);if(count&&!bytes(read,items,pointers.data(),pointers.size()*4))return false;
 std::vector<Entry> next;next.reserve(count);
 for(int n=0;n<count;++n){uint32_t p=pointers[n],owner=0,t=0;uint8_t present=0,ready=0,suspended=0,drain=0,manualControl=0;
  if(!p)continue;
  if(p<0x10000||!get(read,p+0x2C,owner)||!get(read,p+0x6D,present))return false;
  if(owner!=house||!present)continue;
  if(!get(read,p+0x28,t)||t<0x10000)return false;
  Entry e;e.instance=p;e.type=t;e.house=house;e.index=n;
  if(!bytes(read,t+0x24,e.id,24)||!std::memchr(e.id,0,24)||!e.id[0]||!get(read,t+0xF5,manualControl))return false;
  if(manualControl)continue;
  uint32_t name=0;get(read,t+0x60,name);if(name>=0x10000)bytes(read,name,e.name,sizeof e.name-2);e.name[40]=0;
  if(!e.name[0])for(int j=0;j<24&&e.id[j];++j)e.name[j]=wchar_t(uint8_t(e.id[j]));
  int start=-1,left=0,total=-1,drainState=0;
  if(!get(read,p+0x30,start)||!get(read,p+0x38,left)||!get(read,p+0x24,total)||!get(read,p+0x6F,ready)||!get(read,p+0x70,suspended)||!get(read,t+0xE5,drain))return false;
  if(total==-1&&!get(read,t+0xB0,total))return false;
  if(drain&&(!get(read,p+0x7C,drainState)||drainState<0||drainState>2))return false;
  e.frames=int(std::clamp<int64_t>(int64_t(left)-(start<0?0:std::max<int64_t>(0,int64_t(frame)-start)),0,INT32_MAX));
  e.suspended=suspended!=0;e.draining=drain&&drainState==2;e.ready=!e.suspended&&(drain?drainState!=0:ready!=0);
  e.percent=e.ready?100:total>0?int(std::clamp<int64_t>((int64_t(total)-e.frames)*100/total,0,100)):0;
  get(read,t+0xB8,e.shape);next.push_back(e);
 }
 uint32_t itemsAfter=0;int countAfter=0,capacityAfter=0;
 if(!get(read,0xA83D4C,houseAfter)||houseAfter!=house||!get(read,0xA8ED84,frameAfter)||frameAfter<frame||frameAfter-frame>2||
    !get(read,house+0x258,itemsAfter)||itemsAfter!=items||!get(read,house+0x264,countAfter)||countAfter!=count||
    !get(read,house+0x25C,capacityAfter)||capacityAfter!=capacity)return false;
 std::vector<uint32_t> after(size_t(count),0);if(count&&(!bytes(read,items,after.data(),after.size()*4)||after!=pointers))return false;
 for(const auto&e:next){uint32_t owner=0,type=0;uint8_t present=0;
  if(!get(read,e.instance+0x2C,owner)||owner!=house||!get(read,e.instance+0x28,type)||type!=e.type||!get(read,e.instance+0x6D,present)||!present)return false;
 }
 out=std::move(next);mapRight=transform.active?transform.physical.x+transform.physical.w:viewX+viewW;return true;
}

}
