#pragma once
#include <cstdint>
#include <map>
#include <vector>
namespace superhot {
struct Range { int id, first, count; };
// Verified vanilla YR table. Paradrop (47) deliberately keeps its X anchor.
inline const std::vector<Range>& ranges(){
 static const std::vector<Range> r={{50,279,20},{53,319,10},{57,346,5},{58,357,12},
  {79,450,10},{80,460,10},{81,470,10},{82,480,8},{83,488,8},{84,496,8},{85,504,8},
  {50,514,1},{53,513,1},{83,516,1},{84,515,1},{85,512,1}};
 return r;
}
struct Point { int x=0,y=0; bool valid=false; };
using Centers=std::map<int,Point>;
// Calculate once per source/style, using the union across the animation.
template<class Frames>
Centers prepare(const Frames& frames,int w,int h){
 Centers out;
 for(const auto& r:ranges()){
  if(r.first<0||r.count<1||r.first+r.count>int(frames.size()))continue;
  int left=w,top=h,right=-1,bottom=-1;
  for(int f=r.first;f<r.first+r.count;++f)for(int y=0;y<h;++y)for(int x=0;x<w;++x){
   if((frames[f][size_t(y)*w+x]>>24)==0)continue;
   if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;
  }
  if(right>=left&&bottom>=top)out[r.first]={(left+right+1)/2,(top+bottom+1)/2,true};
 }
 return out;
}
inline Point resolve(const Centers& centers,int id,int frame,int first,int count){
 for(const auto& r:ranges())if(r.id==id&&r.first==first&&r.count==count&&frame>=first&&frame<first+count){
  auto i=centers.find(first);if(i!=centers.end())return i->second;
 }
 return {};
}
}
