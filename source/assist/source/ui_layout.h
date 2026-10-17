#pragma once
#include <algorithm>
namespace assist { namespace uilayout {
constexpr int Width=685,BodyTop=124,FooterHeight=84;
struct Rect {int x,y,w,h;};
struct Placement {int page;Rect rect;};
inline Placement legacy(int id,Rect r){
 int page=-1;
 if(id>=430&&id<=433){page=-2;r.y=id==430?0:46;}
 else if(r.y>=1130){page=2;r.y-=1030;}
 else if(r.y>=930){page=1;r.y-=820;}
 else if(r.y>=657){page=4;r.y-=547;}
 else if(r.y>=501){page=3;r.y-=391;}
 else if(r.y>=84)page=0;
 if(page>=0)r.y+=8;
 return {page,r};
}
inline int footerTop(int height){return std::max(BodyTop+40,height-FooterHeight);}
inline int bodyBottom(int height){return footerTop(height)-10;}
inline int maxScroll(int contentBottom,int height){return std::max(0,contentBottom-bodyBottom(height));}
inline Rect visible(Rect base,int page,int height,int width,int scroll){
 base.x+=std::max(0,(width-Width)/2);
 if(page>=0)base.y-=scroll;else if(page==-2)base.y+=footerTop(height);
 return base;
}
}}
