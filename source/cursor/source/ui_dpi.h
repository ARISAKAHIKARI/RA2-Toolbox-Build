#pragma once
// Win32-independent helpers: layout stored in 96-DPI logical pixels.
#include <algorithm>
#include <cstdint>
namespace uidpi {
struct Rect {int x=0,y=0,w=0,h=0;};
struct View {int x=0,y=0,contentW=0,contentH=0;};
inline unsigned validDpi(unsigned dpi){return dpi>=48&&dpi<=768?dpi:96;}
inline int scale(int px,unsigned dpi){
 const int64_t n=int64_t(px)*validDpi(dpi);
 return int(n>=0?(n+48)/96:(n-48)/96);
}
inline int rescale(int px,unsigned from,unsigned to){
 const int64_t n=int64_t(px)*validDpi(to);
 const int64_t d=validDpi(from);
 return int(n>=0?(n+d/2)/d:(n-d/2)/d);
}
inline Rect place(const Rect& logical,unsigned dpi,int scrollX,int scrollY){
 return {scale(logical.x,dpi)-scrollX,scale(logical.y,dpi)-scrollY,
         std::max(1,scale(logical.w,dpi)),std::max(1,scale(logical.h,dpi))};
}
inline View scroll(int x,int y,int clientW,int clientH,int baseW,int baseH,unsigned dpi){
 int contentW=std::max(1,scale(baseW,dpi));
 int contentH=std::max(1,scale(baseH,dpi));
 return {std::clamp(x,0,std::max(0,contentW-std::max(clientW,0))),
         std::clamp(y,0,std::max(0,contentH-std::max(clientH,0))),contentW,contentH};
}
}
