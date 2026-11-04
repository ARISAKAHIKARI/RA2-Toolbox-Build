#pragma once
#include "overlay_composition.h"
namespace overlaycomposition {
inline bool groupIds(const std::vector<uint64_t>&ids){
 if(ids.empty()||ids.size()>16)return false;
 for(size_t i=0;i<ids.size();++i){if(!ids[i])return false;for(size_t j=0;j<i;++j)if(ids[i]==ids[j])return false;}return true;
}
inline bool groupValid(const std::vector<uint64_t>&ids,const std::vector<Surface>&layers,const Context&context){
 if(!groupIds(ids)||layers.size()>ids.size())return false;
 for(size_t i=0;i<layers.size();++i){
  if(!valid(layers[i],context)||std::find(ids.begin(),ids.end(),layers[i].id)==ids.end())return false;
  for(size_t j=0;j<i;++j)if(layers[i].id==layers[j].id)return false;
 }return true;
}
}
