#pragma once
#include "catalog.h"
namespace hooktest {
// Excluding a group also excludes every group sharing its native sites.
// Dependencies stay atomic: never install half a group or silently re-add
// a suspect site through a later test's dependencies.
struct Selection {
 std::vector<bool> excluded=std::vector<bool>(steps().size(),false);
 int blockedBy(unsigned index)const{
  if(index<2||index>=steps().size())return -1;
  for(unsigned j=2;j<steps().size();++j)if(excluded[j]){
   if(index==j)return int(j);
   for(auto a:steps()[index].sites)if(std::find(steps()[j].sites.begin(),steps()[j].sites.end(),a)!=steps()[j].sites.end())return int(j);
  }
  return -1;
 }
 std::vector<unsigned> schedule(int choice,bool following,bool baselines=false)const{
  std::vector<unsigned> result;if(baselines)result={0,1};
  if(choice<0||choice>=int(steps().size()-1))return {};
  unsigned first=choice?unsigned(choice+1):2,last=choice&&!following?first+1:unsigned(steps().size());
  for(unsigned i=first;i<last;++i)if(blockedBy(i)<0)result.push_back(i);
  return result;
 }
 int nextChoice(unsigned after)const{
  for(unsigned i=after+1;i<steps().size();++i)if(blockedBy(i)<0)return int(i)-1;
  return -1;
 }
};
// Stable persisted names rather than localized labels or catalogue positions.
inline const wchar_t*selectionKey(Kind k){
 switch(k){
  case FrameSignal:return L"FrameSignal";case Tooltip:return L"Tooltip";case Mega:return L"Mega";
  case Placement:return L"Placement";case Camera:return L"Camera";case LowDamage:return L"LowDamage";
  case AirType:return L"AirType";case AirFull:return L"AirFull";case Control:return L"Control";
  case ShroudClear:return L"ShroudClear";case ShroudHalf:return L"ShroudHalf";case TechPair:return L"TechPair";
  case TechMaster:return L"TechMaster";case DirectPlacement:return L"DirectPlacement";case Network:return L"Network";
  case CommandOwner:return L"CommandOwner";case Sell:return L"Sell";case Crc:return L"Crc";case DirectMap:return L"DirectMap";
  default:return L"Baseline";
 }
}
}
