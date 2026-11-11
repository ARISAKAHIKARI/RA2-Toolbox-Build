#pragma once
#include "overlay_composition.h"
namespace sparsepatch {
using overlaycomposition::Rect;
// Transparent complement of the replacement rectangles, with native UI holes.
// nullptr is failure, NEVER an instruction to publish an unmasked full scene.
inline std::shared_ptr<const std::vector<Rect>> mask(int width,int height,
 const std::vector<Rect>&keep,const std::vector<Rect>&holes={}){
 if(width<1||height<1||width>8192||height>8192||uint64_t(width)*height>16*1024*1024||keep.size()>2048||holes.size()>16384)return {};
 using Span=std::pair<int,int>;
 std::vector<std::vector<Span>> rows(height),cuts(height);
 auto add=[&](const std::vector<Rect>&rects,auto&target){for(auto r:rects){r=overlaycomposition::intersection(r,{0,0,width,height});
  for(int y=r.y;y<r.y+r.h;++y)target[y].emplace_back(r.x,r.x+r.w);
 }};
 add(keep,rows);add(holes,cuts);
 auto result=std::make_shared<std::vector<Rect>>();result->reserve(height*2);
 for(int y=0;y<height;++y){
  auto&visible=rows[y];auto&hidden=cuts[y];std::sort(visible.begin(),visible.end());int end=0;
  for(auto s:visible){if(s.first>end)hidden.emplace_back(end,s.first);end=std::max(end,s.second);}
  if(end<width)hidden.emplace_back(end,width);
  std::sort(hidden.begin(),hidden.end());
  for(auto s:hidden){
   if(!result->empty()&&result->back().y==y&&s.first<=result->back().x+result->back().w){auto&last=result->back();last.w=std::max(last.x+last.w,s.second)-last.x;}
   else{if(result->size()>=16384)return {};result->push_back({s.first,y,s.second-s.first,1});}
  }
 }
 return result;
}
}
