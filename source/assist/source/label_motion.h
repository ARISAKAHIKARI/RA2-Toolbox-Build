#pragma once
#include "protocol.h"
#include <algorithm>
#include <cmath>
#include <stdint.h>
namespace assist { namespace visual {
constexpr int DefaultFPS=500;
inline int initialFPS(int stored,int revision){return revision<1&&stored<=60?DefaultFPS:std::clamp(stored,5,500);}
struct LabelView {uint32_t id;float x,y;int used,capacity,centis,shield,advanced;uint32_t ownerColor;int frozenCentis;};
inline float blend(float a,float b,uint32_t now,uint32_t start,uint32_t span){
 float f=span?std::clamp(float(int32_t(now-start))/span,0.f,1.f):1.f;return a+(b-a)*f;
}
inline float grid(float n){return std::round(n*8.f)/8.f;}
class Motion {
 struct State {Label to{};float x{},y{},iron{};uint32_t tick{},span{};};
 State banks[2][MaxLabels]{};int hash[MaxLabels*2]{};int bank=0,count=0;
 uint32_t match=0,tick=0,camTick=0,camSpan=0;int frame=0;bool initialized=false;
 float camFromX=0,camFromY=0;int camX=0,camY=0;
 static unsigned slot(uint32_t id){return (id*2654435761u)&(MaxLabels*2-1);}
public:
 void clear(){initialized=false;count=0;}
 void update(const Snapshot&s){update(s,s.frame);}
 void update(const Snapshot&s,int displayFrame){
  uint32_t now=s.sampledTick;bool reset=!initialized||match!=s.match||displayFrame<frame;
  bool newFrame=reset||displayFrame!=frame;uint32_t span=reset?0:std::clamp(uint32_t(now-tick),8u,100u);
  float cameraX=reset?float(s.cameraX):blend(camFromX,float(camX),now,camTick,camSpan);
  float cameraY=reset?float(s.cameraY):blend(camFromY,float(camY),now,camTick,camSpan);
  if(reset||camX!=s.cameraX||camY!=s.cameraY){
   bool jump=reset||std::abs(s.cameraX-cameraX)>96||std::abs(s.cameraY-cameraY)>96;
   camSpan=0;camTick=now; // camera offsets belong to the current viewport
   camFromX=cameraX;camFromY=cameraY;camX=s.cameraX;camY=s.cameraY;
  }
  for(int&i:hash)i=-1;
  if(!reset)for(int i=0;i<count;++i){unsigned h=slot(banks[bank][i].to.id);while(hash[h]!=-1)h=(h+1)&(MaxLabels*2-1);hash[h]=i;}
  int next=1-bank,n=std::clamp(s.labels,0,MaxLabels);
  for(int i=0;i<n;++i){const Label&label=s.label[i];State state{};state.to=label;
   float x=float(label.x)+s.cameraX,y=float(label.y)+s.cameraY;
   state.to.x=int(x);state.to.y=int(y);state.x=x;state.y=y;state.iron=float(label.ironFrames);state.tick=now;
   int old=-1;unsigned h=slot(label.id);
   if(!reset)while(hash[h]!=-1){int j=hash[h];if(banks[bank][j].to.id==label.id){old=j;break;}h=(h+1)&(MaxLabels*2-1);}
   if(old>=0){const State&p=banks[bank][old];
    if(!newFrame&&p.to.x==state.to.x&&p.to.y==state.to.y&&p.to.ironFrames==label.ironFrames&&p.to.shield==label.shield){
     state=p;state.to.used=label.used;state.to.capacity=label.capacity;state.to.advanced=label.advanced;state.to.ownerColor=label.ownerColor;state.to.frozenFrames=label.frozenFrames;
    }else{
     float px=blend(p.x,float(p.to.x),now,p.tick,p.span),py=blend(p.y,float(p.to.y),now,p.tick,p.span);
     if(std::abs(x-px)<=8&&std::abs(y-py)<=8){state.x=px;state.y=py;state.span=newFrame?span:std::clamp(uint32_t(now-p.tick),2u,16u);}
     if(label.ironFrames>0&&p.to.ironFrames>0&&label.shield==p.to.shield&&label.ironFrames<=p.to.ironFrames&&int64_t(p.to.ironFrames)-label.ironFrames<=int64_t(displayFrame)-frame){
      state.iron=blend(p.iron,float(p.to.ironFrames),now,p.tick,p.span);state.span=span;
     }
    }
   }banks[next][i]=state;
  }
  bank=next;count=n;match=s.match;frame=displayFrame;if(newFrame)tick=now;initialized=true;
 }
 int render(uint32_t now,LabelView*out)const{
  float cameraX=blend(camFromX,float(camX),now,camTick,camSpan),cameraY=blend(camFromY,float(camY),now,camTick,camSpan);
  for(int i=0;i<count;++i){const State&s=banks[bank][i];float iron=s.to.ironFrames>0?blend(s.iron,float(s.to.ironFrames),now,s.tick,s.span):0;
   int centis=iron>0?int(std::min(2147483647.0,std::max(1.0,std::round(double(iron)*100.0/15.0)))):0;
   out[i]={s.to.id,grid(blend(s.x,float(s.to.x),now,s.tick,s.span)-cameraX),grid(blend(s.y,float(s.to.y),now,s.tick,s.span)-cameraY),s.to.used,s.to.capacity,centis,s.to.shield,s.to.advanced,s.to.ownerColor,int(std::min<int64_t>(2147483647,int64_t(s.to.frozenFrames)*100/15))};
  }return count;
 }
};
} }
