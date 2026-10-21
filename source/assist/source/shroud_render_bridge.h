#include "shroud_raster.h"
// Reuses the two manager-owned mask boundaries, after native SHP loading.
// Writes only the local alpha render buffer, never Cell visibility/occupation.
static bool halfShroud(uint32_t context,bool fog){
 if(context<0x10000)return false;
 struct Point {int x,y;};struct Rect {int x,y,w,h;};
 const uint32_t args=fog?context+0x30:context+8;
 uint32_t shape=fog?get<uint32_t>(0x89E760):get<uint32_t>(context-4);
 uint32_t point=get<uint32_t>(args),clip=get<uint32_t>(args+4);int frame=get<int>(args+8);
 uint32_t alpha=get<uint32_t>(0x87E8A4);
 if(shape<0x10000||point<0x10000||clip<0x10000||alpha<0x10000||frame<0||frame>4095)return false;
 Point p=get<Point>(point);Rect c=get<Rect>(clip),r{};
 if(c.w<=0||c.h<=0||c.w>32768||c.h>32768||std::abs(p.x)>65536||std::abs(p.y)>65536)return false;
 call<uint32_t>(0x69E7E0,shape,&r,frame);
 if(r.w<=0||r.h<=0||r.w>4096||r.h>4096||std::abs(r.x)>8192||std::abs(r.y)>8192)return false;
 uint32_t pixels=call<uint32_t>(0x69E740,shape,frame);
 const uint32_t end=get<uint32_t>(alpha+0x1C),bytes=get<uint32_t>(alpha+0x20);
 const int stride=get<int>(alpha+0x28);
 if(pixels<0x10000||bytes<2||bytes>128*1024*1024||end<bytes+0x10000||stride<=0||stride>32768||uint32_t(stride)*2>bytes)return false;
 const uint32_t begin=end-bytes;
 int x0=std::max(p.x+r.x,c.x),y0=std::max(p.y+r.y,c.y);
 int x1=std::min(p.x+r.x+r.w,c.x+c.w),y1=std::min(p.y+r.y+r.h,c.y+c.h);
 if(x1<=x0||y1<=y0)return true;
 uint32_t row=call<uint32_t>(0x4114B0,alpha,x0,y0-get<int>(0x886FA4));
 if(row<begin||row>=end||(row&1))return false;
 for(int y=y0;y<y1;++y){
  uint32_t dst=row,src=pixels+uint32_t((y-p.y-r.y)*r.w+x0-p.x-r.x);
  for(int x=x0;x<x1;++x){
   uint8_t mask=get<uint8_t>(src++);uint16_t previous=get<uint16_t>(dst);
   if(mask<=127)put<uint16_t>(dst,shroudraster::half(mask,previous,fog));
   dst+=2;if(dst>=end)dst-=bytes;
  }
  row+=uint32_t(stride)*2;if(row>=end)row-=bytes;
 }
 return true;
}
