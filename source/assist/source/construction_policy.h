#pragma once
#include <vector>
#include <algorithm>
namespace assist { namespace construction {
inline bool prerequisite(int index,const std::vector<int>&owned,const std::vector<int>*groups,int future=-1){
 auto has=[&](int n){return n>=0&&n<int(owned.size())&&(owned[n]>0||n==future);};
 if(index>=0)return has(index);if(index< -6)return false;
 for(int n:groups[-index-1])if(has(n))return true;return false;
}
inline bool available(const std::vector<int>&needs,const std::vector<int>&overrides,const std::vector<int>&owned,const std::vector<int>*groups,int future=-1){
 for(int n:overrides)if(prerequisite(n,owned,groups,future))return true;
 for(int n:needs)if(!prerequisite(n,owned,groups,future))return false;return true;
}
}}
