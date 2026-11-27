#pragma once
// Persistent *UI catalogue* only. Never influences the running game's types,
// orders, simulation state, or hook/bridge layout.
#include "protocol.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <iterator>

namespace typecatalog {
constexpr size_t MaxSavedTypes=2048;

inline bool validId(std::string_view id) {
 if(id.empty()||id.size()>=sizeof(assist::Type::id))return false;
 for(unsigned char c:id)if(c<=32||c>=127||c=='='||c=='|'||c=='['||c==']')return false;
 return true;
}
inline std::string_view idView(const assist::Type& t){
 const void* end=std::memchr(t.id,0,sizeof t.id);
 return end?std::string_view(t.id,static_cast<const char*>(end)-t.id):std::string_view{};
}
inline std::string idOf(const assist::Type& t){return std::string(idView(t));}
inline bool validType(const assist::Type& t){
 auto id=idView(t);
 return validId(id)&&std::find(std::begin(t.name),std::end(t.name),uint16_t(0))!=std::end(t.name);
}
inline assist::Type placeholder(const std::string& id){
 assist::Type t{};
 t.kind=-1;t.index=-1;
 if(!validId(id))return t;
 std::memcpy(t.id,id.data(),id.size());
 for(size_t i=0;i<id.size()&&i<std::size(t.name)-1;++i)t.name[i]=uint16_t(static_cast<unsigned char>(id[i]));
 return t;
}
struct Catalogue {
 std::vector<assist::Type> types;
 int find(std::string_view id,int kind=-1)const{
  for(size_t i=0;i<types.size();++i){
   if((kind<0||types[i].kind==kind)&&idView(types[i])==id)return int(i);
  }
  return -1;
 }
 // Return the modified row to the persistence caller without a second scan.
 // An unchanged or rejected entry leaves the optional result at -1.
 bool remember(const assist::Type& t,int* changedRow=nullptr){
  if(changedRow)*changedRow=-1;
  if(!validType(t))return false;
  const auto id=idView(t);
  int i=-1,placeholderRow=-1;
  for(size_t j=0;j<types.size();++j){
   const auto&old=types[j];
   if(idView(old)!=id)continue;
   if(t.kind<0||old.kind==t.kind){i=int(j);break;}
   if(old.kind==-1&&placeholderRow<0)placeholderRow=int(j);
  }
  if(i<0)i=placeholderRow; // Upgrade an offline placeholder only.
  if(i>=0){
   assist::Type& old=types[i];
   if(old.kind==t.kind&&old.index==t.index&&old.cost==t.cost&&old.naval==t.naval&&
      !std::memcmp(old.name,t.name,sizeof old.name))return false;
   old=t;
   if(changedRow)*changedRow=i;
   return true;
  }
  if(types.size()>=MaxSavedTypes)return false;
  types.push_back(t);
  if(changedRow)*changedRow=int(types.size()-1);
  return true;
 }
 bool ensure(const std::string& id){
  if(!validId(id)||find(id)>=0||types.size()>=MaxSavedTypes)return false;
  types.push_back(placeholder(id));return true;
 }
};
}
