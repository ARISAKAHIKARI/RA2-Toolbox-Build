#pragma once
#include <stdint.h>
#include <stddef.h>
namespace assist { namespace offlinetools {
enum Mode : uint32_t {None=0,Repair=1,Sell=2};
constexpr bool offline(int session){return session==0||session==5;}
inline Mode mode(const uint8_t (&tools)[5]){
 for(auto v:tools)if(v>1)return None;
 if(tools[2]||tools[3]||tools[4]||tools[0]==tools[1])return None;
 return tools[0]?Repair:Sell;
}
inline int rejectedCursor(Mode m){return m==Repair?35:m==Sell?32:-1;}
inline int acceptedCursor(Mode m){return m==Repair?33:m==Sell?31:-1;}
template<class Read> Mode current(Read read){
 uint8_t tools[5]{};int session=-1;
 if(!read(0xA8B238,&session,4)||!offline(session)||!read(0x880998,tools,sizeof tools))return None;
 return mode(tools);
}
// Only the documented YR MouseClass layout is admitted. Unknown/legacy
// cursor layouts leave both native tools entirely untouched.
template<class Read> bool denied(Mode m,Read read){
 uint32_t state[2]{};
 return m!=None&&read(0x87F7E8+0x555C,state,sizeof state)&&(state[0]&255)<=1&&state[1]==uint32_t(rejectedCursor(m));
}
struct Point {
 uint32_t match=0,tick=0,house=0,generation=0,tool=0;
 int32_t frame=-1,x=0,y=0,width=0,height=0,cameraX=0,cameraY=0;
};
struct Hint {Point point;uint32_t pointer=0,id=0;};
inline bool samePoint(const Point&a,const Point&b){
 const int64_t dx=int64_t(a.x)-b.x,dy=int64_t(a.y)-b.y;
 return a.match&&a.match==b.match&&a.house==b.house&&a.generation==b.generation&&
 a.tool==b.tool&&a.width==b.width&&a.height==b.height&&a.cameraX==b.cameraX&&a.cameraY==b.cameraY&&
 dx>=-2&&dx<=2&&dy>=-2&&dy<=2;
}
inline bool usable(const Hint&h,const Point&p,uint32_t now){
 return h.pointer>=0x10000&&h.id&&samePoint(h.point,p)&&p.frame>=h.point.frame&&
 uint32_t(now-h.point.tick)<=100&&uint32_t(now-p.tick)<=100;
}
inline bool repairTarget(int hp,int maximum,bool repairing,bool repairable,bool clickable){
 return hp>0&&maximum>0&&(!repairable||!clickable)&&(repairing||hp<maximum);
}
inline bool sellTarget(bool owned,bool active,bool drained,bool eligibleCell){
 return owned&&active&&!drained&&eligibleCell;
}
struct Ticket {uint32_t id=0,pointer=0,tick=0;int frame=-1;bool repairing=false;Mode mode=None;};
struct Tickets {
 uint32_t match=0;Ticket slots[32]{};
 void clear(){match=0;for(auto&s:slots)s={};}
 Ticket* admit(uint32_t epoch,uint32_t id,uint32_t pointer,Mode mode,bool repairing,int frame,uint32_t now){
  if(match!=epoch){clear();match=epoch;}Ticket*free=nullptr;
  for(auto&s:slots){
   if(s.id==id&&s.pointer==pointer){
    if(s.mode==Repair&&s.repairing!=repairing){s={};}
    else if(frame>=s.frame&&frame-s.frame>=8&&uint32_t(now-s.tick)>=2000){s={};}
    else return nullptr;
   }else if(s.id&&frame>=s.frame&&frame-s.frame>=8&&uint32_t(now-s.tick)>=2000){s={};}
   if(!s.id&&!free)free=&s;
  }
  return free;
 }
};
}}
