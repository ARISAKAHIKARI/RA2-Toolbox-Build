#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace assist { namespace statusbadges {
// All sizes are game pixels, independent of status-text zoom.
constexpr int SpiderSize=14,BuffSize=9;
struct Sprite {int width=0,height=0;std::array<uint32_t,SpiderSize*SpiderSize> pixels{};};
template<int W,int H> Sprite raster(const char*const(&mask)[H],uint32_t color){
 Sprite s;s.width=W;s.height=H;
 for(int y=0;y<H;++y)for(int x=0;x<W;++x){
  bool ink=mask[y][x]=='#',edge=false;
  if(!ink)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){int xx=x+dx,yy=y+dy;if(xx>=0&&xx<W&&yy>=0&&yy<H&&mask[yy][xx]=='#')edge=true;}
  s.pixels[y*W+x]=ink?color:edge?0xFF07101Bu:0;
 }return s;
}
inline Sprite spider(uint32_t color){
 static const char*const mask[]={"..............","..#........#..","...#..##..#...","....######....",".###..##..###.","....######....",".....####.....",".####.##.####.","....######....","...#..##..#...","..#...##...#..",".#..........#.","..............",".............."};
 return raster<14,14>(mask,color);
}
inline Sprite buff(unsigned index){
 static const char*const fire[]={"...#...","..##...","..###..",".#####.",".#####.","..###..","......."};
 static const char*const armor[]={".......",".#####.",".#...#.",".#...#.","..#.#..","...#...","......."};
 static const char*const speed[]={".......",".##.##.","..##.##","...#..#","..##.##",".##.##.","......."};
 return index==0?raster<7,7>(fire,0xFFFFBB55u):index==1?raster<7,7>(armor,0xFF79CBFFu):raster<7,7>(speed,0xFF89E299u);
}
struct Point {float x=0,y=0;};
struct Layout {Point center;Point icons[3];unsigned kinds[3]{};int count=0;};
// Projected cell is a 60x30 diamond. Fit the complete 9x9 tile (including
// outline), not just its center, inside that diamond with a subpixel margin.
inline Layout layout(float x,float y,float cellX,float cellY,uint32_t buffs,bool parasite){
 Layout r;r.center={cellX,cellY};
 for(unsigned i=0;i<3;++i)if(buffs&(1u<<i))r.kinds[r.count++]=i;
 float minU=1e6f,maxU=-1e6f,minV=1e6f,maxV=-1e6f;
 for(int i=0;i<r.count;++i){
  float dx=(i-(r.count-1)*.5f)*11.f,dy=parasite?(r.count==3&&i!=1?4.5f:9.f):8.f;
  r.icons[i]={dx,dy};float u=dx/30.f+dy/15.f,v=dx/30.f-dy/15.f;
  minU=std::min(minU,u);maxU=std::max(maxU,u);minV=std::min(minV,v);maxV=std::max(maxV,v);
 }
 if(!r.count)return r;
 // Clamp the group in diamond coordinates; keep spacing and icon size intact.
 constexpr float room=1.f-BuffSize/60.f-BuffSize/30.f-.025f;
 float u=std::clamp((x-cellX)/30.f+(y-cellY)/15.f,-room-minU,room-maxU);
 float v=std::clamp((x-cellX)/30.f-(y-cellY)/15.f,-room-minV,room-maxV);
 for(int i=0;i<r.count;++i){r.icons[i].x+=cellX+15.f*(u+v);r.icons[i].y+=cellY+7.5f*(u-v);
 }return r;
}
}}
