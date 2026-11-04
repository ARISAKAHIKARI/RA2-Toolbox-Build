#pragma once
#include <algorithm>
#include <set>
#include <vector>
namespace assist { namespace settingsfold {
struct Item {int page,y,height,group;bool hidden=false;};
struct Placed {int y;bool hidden;};
// Reflow complete overlapping rows, preserving labels, combo dropdown sizes,
// and list/button columns. Only visibility and coordinates change: children
// keep their HWNDs, saved values, feature state and dependency decisions.
inline std::vector<Placed> layout(const std::vector<Item>&items,const std::set<int>&opened){
 std::vector<Placed> result;std::set<int>pages;for(const auto&i:items){result.push_back({i.y,false});if(i.page>=0)pages.insert(i.page);}
 for(int page:pages){std::vector<size_t>order;for(size_t i=0;i<items.size();++i)if(items[i].page==page)order.push_back(i);
  std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return items[a].y<items[b].y;});int removed=0;
  for(size_t at=0;at<order.size();){size_t end=at+1;const int top=items[order[at]].y;int bottom=top+items[order[at]].height;
   while(end<order.size()&&items[order[end]].y<bottom){const auto&i=items[order[end++]];bottom=std::max(bottom,i.y+i.height);}
   bool shown=false;for(size_t n=at;n<end;++n){const auto&i=items[order[n]];shown|=!i.hidden&&(!i.group||opened.count(i.group)!=0);}
   for(size_t n=at;n<end;++n){const auto&i=items[order[n]];result[order[n]]={i.y-removed,i.hidden||(i.group&&opened.count(i.group)==0)};}
   if(!shown)removed+=bottom-top+14;at=end;
  }
 }
 return result;
}
}}
