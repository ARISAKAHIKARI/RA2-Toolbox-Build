#pragma once
#include "core.h"
#include <unordered_map>
namespace um {
class TypeIndex {
 std::unordered_map<uint32_t,int> indices_;
public:
 void clear(){indices_.clear();}
 void rebuild(const Snapshot&s){indices_.clear();indices_.reserve(size_t(s.typeCount));for(int i=0;i<s.typeCount;++i)indices_.try_emplace(s.types[i].ptr,i);}
 int find(uint32_t p)const{auto i=indices_.find(p);return i==indices_.end()?-1:i->second;}
};
}
