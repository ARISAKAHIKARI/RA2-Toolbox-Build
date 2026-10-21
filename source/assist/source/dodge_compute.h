#pragma once
#include <cstdint>
#include <climits>
namespace assist { namespace dodgecompute {
constexpr int MaxSources=32,MaxThreats=8,MaxCells=16;
struct Threat {uint32_t id=0,pointer=0;int x=0,y=0,kind=0,vx=0,vy=0;};
struct Cell {uint32_t cell=0;int x=0,y=0;};
struct Source {
 uint32_t id=0,pointer=0,token=0,job=0,destination=0,freshShot=0;
 int x=0,y=0,frame=0,sentFrame=0,moves=0,count=0,cells=0;
 uint32_t restoreTarget=0,restoreDestination=0;int restoreMission=5,restoreTag=0;
 Threat threats[MaxThreats];Cell options[MaxCells];
};
struct Decision {int mission=-1;uint32_t target=0,destination=0;int tag=0;uint32_t done=0;};
struct Input {uint32_t match=0,tick=0;int frame=-1,count=0;Source sources[MaxSources];};
struct Result {uint32_t match=0,tick=0;int frame=-1,count=0;Decision decisions[MaxSources];};
inline int64_t distance(int ax,int ay,int bx,int by){int64_t x=int64_t(ax)-bx,y=int64_t(ay)-by;return x*x+y*y;}
// Air threats carry the observed impact point and horizontal flight direction.
// Prefer crossing the flight line; moving farther along it can still be hit.
inline int64_t clearance(int x,int y,const Threat&t){
 int64_t dx=int64_t(x)-t.x,dy=int64_t(y)-t.y;
 if(dx < -16384||dx>16384||dy < -16384||dy>16384)return 0;
 int64_t radial=dx*dx+dy*dy;
 if(t.kind!=2)return radial;
 if(t.vx < -256||t.vx>256||t.vy < -256||t.vy>256)return 0;
 int64_t norm=int64_t(t.vx)*t.vx+int64_t(t.vy)*t.vy;
 if(!norm)return radial; // vertical bomb: leave its impact area
 int64_t cross=dx*t.vy-dy*t.vx;
 return radial+4*(cross*cross/norm);
}
inline Decision decide(const Source&s){
 Decision d;if(s.count<0||s.count>MaxThreats||s.cells<0||s.cells>MaxCells)return d;
 if(!s.count){
  if(s.job&&s.frame-s.sentFrame>=6){d.mission=s.restoreMission;d.target=s.restoreTarget;d.destination=s.restoreDestination;d.tag=s.restoreTag;d.done=1;}return d;
 }
 if(s.job&&!s.freshShot&&s.frame-s.sentFrame<12)return d;
 if(!s.freshShot&&s.moves>=3){if(s.frame-s.sentFrame>=60)d.done=1;return d;}
 int64_t base=LLONG_MAX,best=LLONG_MIN;int choice=-1;
 for(int n=0;n<s.count;++n)base=base<clearance(s.x,s.y,s.threats[n])?base:clearance(s.x,s.y,s.threats[n]);
 for(int k=0;k<s.cells;++k){const auto&c=s.options[k];if(!c.cell||c.cell==s.destination)continue;int64_t score=LLONG_MAX;
  for(int n=0;n<s.count;++n){auto&t=s.threats[n];int64_t v=clearance(c.x,c.y,t);if(v<score)score=v;}
  if(score>base+4096&&(score>best||(score==best&&(choice<0||c.cell<s.options[choice].cell)))){best=score;choice=k;}
 }
 if(choice>=0){d.mission=2;d.destination=s.options[choice].cell;d.tag=11;}return d;
}
inline Result calculate(const Input&i){Result r;r.match=i.match;r.tick=i.tick;r.frame=i.frame;if(i.count<0||i.count>MaxSources)return r;
 r.count=i.count;for(int n=0;n<i.count;++n)r.decisions[n]=decide(i.sources[n]);return r;}
}}
