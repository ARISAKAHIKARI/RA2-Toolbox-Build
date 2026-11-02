#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "../../shared/source/key_gesture.h"
// Behaviour reference: MagicShiba/MO_ExperienceEnhancements, commit
// 33ed558bd38f36b5012eddcc9c0fb109cc68e38a. Independent implementation;
// this header is pure geometry and preview data, with no engine calls.
namespace assist { namespace moorders {
constexpr uint32_t Fuzzy=1,Area=2,Patrol=4,QuickFormation=8;
constexpr int GestureCount=4;
struct GesturePolicy {
 uint32_t keys[GestureCount]={17,16,'Z',0};
 // Match the existing pointer mouse setting IDs; zero explicitly disables.
 uint32_t buttons[GestureCount]={1,1,1,1};
};
inline int gestureIndex(uint32_t mode){return mode==Fuzzy?0:mode==Area?1:mode==Patrol?2:mode==QuickFormation?3:-1;}
inline uint32_t gestureMode(int index){return index>=0&&index<GestureCount?1u<<index:0;}
inline uint32_t gestureEnabled(uint32_t features,bool formation){return (features&7u)|(formation?QuickFormation:0);}
inline bool gestureButton(uint32_t button){return button==1||button==2||button==3||button==4||button==6;}
inline uint32_t gestureVK(uint32_t button){return button==1?4:button==2?5:button==3?6:button==4?2:button==6?1:0;}
inline bool gestureKey(uint32_t key){
 if(!key)return true;unsigned vk=key&255;
 return vk>=8&&vk<256&&keygesture::isHold(key)&&!(key&~uint32_t(keygesture::Stored));
}
inline uint16_t gestureKeyIdentity(uint32_t key){return keygesture::normalize(key,keygesture::Mode::Hold);}
inline bool sameGestures(const GesturePolicy&a,const GesturePolicy&b){for(int i=0;i<GestureCount;++i)if(a.keys[i]!=b.keys[i]||a.buttons[i]!=b.buttons[i])return false;return true;}
inline uint32_t gestureInvalid(const GesturePolicy&p,uint32_t enabled){
 uint32_t out=0;for(int i=0;i<GestureCount;++i)if((enabled&gestureMode(i))&&p.buttons[i]&&(!gestureButton(p.buttons[i])||!gestureKey(p.keys[i])))out|=gestureMode(i);return out;
}
inline uint32_t gestureConflicts(const GesturePolicy&p,uint32_t enabled){
 uint32_t out=0;for(int i=0;i<GestureCount;++i)if((enabled&gestureMode(i))&&gestureButton(p.buttons[i])&&gestureKey(p.keys[i]))for(int j=i+1;j<GestureCount;++j)
  if((enabled&gestureMode(j))&&p.buttons[i]==p.buttons[j]&&gestureKey(p.keys[j])&&gestureKeyIdentity(p.keys[i])==gestureKeyIdentity(p.keys[j]))out|=gestureMode(i)|gestureMode(j);return out;
}
template<class Keys>inline bool gestureHeld(const Keys&keys,uint32_t binding){
 if(!gestureKey(binding))return false;
 if(binding)return keygesture::matchesHold(keys,binding);
 // Empty means the mouse by itself. Do not special-case a literal native
 // planning key: every keyboard key is excluded from this bare-mouse fallback.
 for(unsigned vk=8;vk<256;++vk)if(keys[vk])return false;return true;
}
inline int gestureSpecificity(uint32_t key){
 if(!key)return 0;unsigned vk=key&255,mods=(key>>8)&7;
 int out=(vk>=16&&vk<=18)?4:8;for(unsigned bit=1;bit<=4;bit<<=1)if(mods&bit)out+=2;return out+((key&keygesture::Double)?1:0);
}
struct GestureResolution {uint32_t mode=0,matched=0,conflicts=0;};
template<class Keys>inline GestureResolution resolveGesture(const GesturePolicy&p,uint32_t enabled,uint32_t button,const Keys&keys){
 GestureResolution result;int best=-1;bool tied=false;
 for(int i=0;i<GestureCount;++i)if((enabled&gestureMode(i))&&p.buttons[i]==button&&gestureButton(button)&&gestureHeld(keys,p.keys[i])){
  const auto mode=gestureMode(i);result.matched|=mode;int score=gestureSpecificity(p.keys[i]);
  if(score>best){best=score;result.mode=mode;tied=false;}else if(score==best)tied=true;
 }
 result.conflicts=gestureConflicts(p,enabled)&result.matched;
 if(result.conflicts||tied){result.conflicts|=tied?result.matched:0;result.mode=0;}return result;
}
struct MouseEdge {uint32_t button=0;bool down=false,up=false;};
inline MouseEdge gestureMouseEdge(uint32_t message,uintptr_t wParam){
 switch(message){
 case 0x201:case 0x203:return {6,true,false};case 0x202:return {6,false,true};
 case 0x204:case 0x206:return {4,true,false};case 0x205:return {4,false,true};
 case 0x207:case 0x209:return {1,true,false};case 0x208:return {1,false,true};
 case 0x20B:case 0x20D:case 0x20C:{unsigned which=unsigned(wParam>>16)&65535;return {which==1?2u:which==2?3u:0u,message!=0x20C,message==0x20C};}
 default:return {};
 }
}
inline uint32_t gestureUpMessage(uint32_t button){return button==1?0x208:button==2||button==3?0x20C:button==4?0x205:button==6?0x202:0;}
// Compatibility helper for old callers; production uses the shared policy.
template<class Keys>inline uint32_t claimsMiddle(uint32_t features,const Keys&keys){return resolveGesture(GesturePolicy{},features&7u,1,keys).mode;}
constexpr int Actors=256,Targets=256,Nodes=64,Waypoints=16,Lines=128;
struct Point {int x=0,y=0,z=0;}; // World leptons in previews, map cells in geometry.
struct Line {Point from,to;};
struct Preview {
 uint32_t match=0,tick=0,active=0;
 int mode=0,count=0,nodeCount=0;
 double zScale=0.0866025403784;
 Line lines[Lines]{};Point nodes[Nodes]{};
};
inline int distance(Point a,Point b){return std::max(std::abs(a.x-b.x),std::abs(a.y-b.y));}
inline void sample(Point*nodes,int&n,Point p,bool force=false){
 if(n&&nodes[n-1].x==p.x&&nodes[n-1].y==p.y)return;
 if(n&&!force&&distance(nodes[n-1],p)<2)return;
 if(n<Nodes)nodes[n++]=p;else nodes[Nodes-1]=p;
}
// Small clicks form a square. An open stroke makes a narrow rectangle; a
// closed freehand stroke preserves its entire shape, resampled to native's
// sixteen-node limit rather than truncating the start of a longer stroke.
inline int patrol(const Point*nodes,int n,Point*out){
 if(!nodes||!out||n<=0)return 0;n=std::min(n,Nodes);
 Point a=nodes[0],b=nodes[n-1];
 if(n>=4&&distance(a,b)<3){
  double lengths[Nodes]{};for(int i=1;i<n;++i)lengths[i]=lengths[i-1]+std::hypot(double(nodes[i].x-nodes[i-1].x),double(nodes[i].y-nodes[i-1].y));
  double closing=std::hypot(double(a.x-b.x),double(a.y-b.y));double total=lengths[n-1]+closing;
  if(total>=8){int count=std::clamp(int(std::ceil(total/3.)),4,Waypoints),used=0,seg=0;
   for(int i=0;i<count;++i){double at=total*i/count;while(seg+1<n&&lengths[seg+1]<at)++seg;Point left=nodes[seg],right=seg+1<n?nodes[seg+1]:a;double len=seg+1<n?lengths[seg+1]-lengths[seg]:closing;double ratio=len>0?(at-lengths[seg])/len:0;Point p{int(std::lround(left.x+(right.x-left.x)*ratio)),int(std::lround(left.y+(right.y-left.y)*ratio)),0};
    if(!used||distance(out[used-1],p))out[used++]=p;
   }if(used>=4)return used;
  }
 }
 if(distance(a,b)<2){out[0]={b.x-2,b.y-2};out[1]={b.x+2,b.y-2};out[2]={b.x+2,b.y+2};out[3]={b.x-2,b.y+2};return 4;}
 double dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);dx/=len;dy/=len;
 const double sideX=-dy*2,sideY=dx*2;
 out[0]={int(std::lround(a.x-dx*2+sideX)),int(std::lround(a.y-dy*2+sideY))};
 out[1]={int(std::lround(b.x+dx*2+sideX)),int(std::lround(b.y+dy*2+sideY))};
 out[2]={int(std::lround(b.x+dx*2-sideX)),int(std::lround(b.y+dy*2-sideY))};
 out[3]={int(std::lround(a.x-dx*2-sideX)),int(std::lround(a.y-dy*2-sideY))};return 4;
}
// Fuzzy attack keeps groups large enough to focus fire. The remainder is
// balanced over the selected targets; no actor is duplicated or omitted.
inline int groups(int actors,int targets,int squad=4){return actors>0&&targets>0?std::clamp(actors/std::max(1,squad),1,targets):0;}
inline int groupTarget(int actor,int actors,int groups){
 if(actor<0||actor>=actors||groups<=0)return -1;int base=actors/groups,extra=actors%groups,wide=(base+1)*extra;
 return actor<wide?actor/(base+1):extra+(actor-wide)/base;
}
}} // assist::moorders
