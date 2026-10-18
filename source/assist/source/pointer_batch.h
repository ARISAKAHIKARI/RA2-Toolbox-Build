#pragma once
#include "pointer_intent.h"
#include <cstring>
namespace assist { namespace pointerinput {
// One physical capture, ordered subrequests, original deadline and identity.
inline bool sameCapture(const Intent&a,const Intent&b){
 return a.tick==b.tick&&a.match==b.match&&a.house==b.house&&a.inputSerial==b.inputSerial&&a.frame==b.frame&&
  a.x==b.x&&a.y==b.y&&a.width==b.width&&a.height==b.height&&a.cameraX==b.cameraX&&a.cameraY==b.cameraY&&
  a.count==b.count&&a.count>=0&&a.count<=MaxSources&&!std::memcmp(a.sources,b.sources,size_t(a.count)*sizeof(Source));
}
struct PendingMatches {
 Intent capture{};MouseRoute routes[(MouseCount+1)]{};int count=0,at=0,accepted=0,rejected=0;
 uint32_t revision=0,cooperativeBefore=0;
 void clear(){count=at=accepted=rejected=0;}
 bool active()const{return count>1;}
 bool waiting()const{return active()&&at<count;}
 void begin(const Intent&i,const MouseRoute*r,int n,uint32_t rev){clear();if(n<1||n>(MouseCount+1))return;capture=i;count=n;revision=rev;for(int j=0;j<n&&j<(MouseCount+1);++j)routes[j]=r[j];}
 Intent next(){if(at>=count)return {};Intent out=capture;const auto&r=routes[at];out.operation=r.operation;out.bindings=r.bindings;
  if(active())out.bindings|=at?BatchNext:BatchStart;++at;return out;
 }
};
}}
