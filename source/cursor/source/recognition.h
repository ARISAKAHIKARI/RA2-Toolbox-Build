#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
// A verified frame range is safer than assuming that the game's cursor-type
// field has an identical layout in every executable / renderer combination.
inline bool cursorRangeMatches(int first,int count,int frame,size_t total){
 return first>=0&&count>=1&&count<=256&&int64_t(first)+count<=int64_t(total)&&frame>=first&&int64_t(frame)<int64_t(first)+count;
}
struct CursorRange {
 int first=0,count=0,key=-1;
};
struct CursorRangeResult {
 int first=0,count=0,key=-1;
 bool inferred=false;
 int choices=0;
 int confirmations=0;
};
struct CursorRangeTracker {
 int previous=-1, candidate=-1, confirmed=-1, streak=0;
 CursorRange locked{};
 void reset(){previous=-1;candidate=confirmed=-1;streak=0;locked={};}
 // Only confirm after TWO adjacent real frame transitions. Never group
 // arbitrary frame changes or merge two ambiguous definitions.
 CursorRangeResult observe(int frame,const std::vector<CursorRange>& definitions,size_t frames){
  CursorRangeResult out;
  if(frame<0||size_t(frame)>=frames){reset();return out;}
  if(frame==previous){if(confirmed>=0){out={locked.first,locked.count,locked.key,true,1,streak};}return out;}
  const int prior=previous;previous=frame;
  if(prior<0){candidate=confirmed=-1;streak=0;return out;}
  CursorRange found{};int unique=0;
  for(const auto& d:definitions){
   if(d.count<2||!cursorRangeMatches(d.first,d.count,frame,frames)||
      !cursorRangeMatches(d.first,d.count,prior,frames)||
      frame!=d.first+(prior-d.first+1)%d.count)continue;
   if(!unique){found=d;unique=1;}
   else if(found.first!=d.first||found.count!=d.count)++unique;
  }
  out.choices=unique;
  if(unique!=1){candidate=confirmed=-1;streak=0;return out;}
  if(candidate==found.key)++streak;
  else{candidate=found.key;streak=1;}
  if(streak>=2){confirmed=found.key;locked=found;out={found.first,found.count,found.key,true,1,streak};}
  return out;
 }
};
