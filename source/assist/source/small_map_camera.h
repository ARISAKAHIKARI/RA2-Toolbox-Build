#pragma once
#include <cstdint>
// Native YR 1.001 camera bounds, verified against 0x6D8640. The inverted
// interval case is the small-map bug: alternating min/max clamps never settle.
// CnCNet's Bugfixes.ClampTacticalPos.cpp documents the same midpoint remedy.
namespace assist { namespace smallcamera {
struct Geometry {int mapWidth,x,y,width,height,viewWidth,viewHeight;};
struct Point {int x,y;};
struct Limits {int64_t left,right,top,bottom;};
inline bool valid(const Geometry&g){return g.mapWidth>0&&g.mapWidth<=1024&&g.width>0&&g.width<=1024&&g.height>0&&g.height<=1024&&g.x>=0&&g.x<=1024&&g.y>=0&&g.y<=1024&&g.viewWidth>0&&g.viewWidth<=32768&&g.viewHeight>0&&g.viewHeight<=32768;}
inline Limits limits(const Geometry&g){
 const int64_t left=g.viewWidth/2+30LL*(2*g.x-g.mapWidth);
 const int64_t top=g.viewHeight/2+15LL*(2*g.y+g.mapWidth-5);
 return {left,left+60LL*g.width-g.viewWidth,top,top+30LL*g.height+135-g.viewHeight};
}
inline bool needed(const Geometry&g){if(!valid(g))return false;auto b=limits(g);return b.right<b.left||b.bottom<b.top;}
inline int bound(int p,int64_t low,int64_t high){if(high<low)return int((low+high)/2);return int(p<low?low:p>high?high:p);}
inline bool clampNative(const Geometry&g,Point&p){
 auto b=limits(g);Point next{int(p.x<b.left?b.left:p.x>b.right?b.right:p.x),int(p.y<b.top?b.top:p.y>b.bottom?b.bottom:p.y)};
 const bool changed=next.x!=p.x||next.y!=p.y;p=next;return changed;
}
inline bool clamp(const Geometry&g,Point&p){
 if(!valid(g))return false;auto b=limits(g);Point next{bound(p.x,b.left,b.right),bound(p.y,b.top,b.bottom)};
 const bool changed=next.x!=p.x||next.y!=p.y;p=next;return changed;
}
}}
