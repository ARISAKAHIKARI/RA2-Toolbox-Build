#pragma once
// Persistent *UI catalogue* only. Never influences the running game's types,
// orders, simulation state, or hook/bridge layout.
#include "protocol.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <iterator>

namespace typecatalog {
constexpr size_t MaxSavedTypes=2048;

inline bool validId(const std::string& id) {
 if(id.empty()||id.size()>=sizeof(assist::Type::id))return false;
 for(unsigned char c:id)if(c<=32||c>=127||c=='='||c=='|'||c=='['||c==']')return false;
 return true;
}
inline std::string idOf(const assist::Type& t){
 const void* end=std::memchr(t.id,0,sizeof t.id);
 return end?std::string(t.id,static_cast<const char*>(end)-t.id):std::string();
}
inline bool validType(const assist::Type& t){
 auto id=idOf(t);
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
 int find(const std::string& id,int kind=-1)const{
  for(size_t i=0;i<types.size();++i){
   if(idOf(types[i])==id&&(kind<0||types[i].kind==kind))return int(i);
  }
  return -1;
 }
 bool remember(const assist::Type& t){
  if(!validType(t))return false;
  std::string id=idOf(t);
  int i=find(id,t.kind);
  if(i<0){for(size_t j=0;j<types.size();++j)if(types[j].kind==-1&&idOf(types[j])==id){i=int(j);break;}} // Upgrade an offline placeholder only.
  if(i>=0){
   assist::Type& old=types[i];
   if(old.kind==t.kind&&old.index==t.index&&old.cost==t.cost&&old.naval==t.naval&&
      !std::memcmp(old.name,t.name,sizeof old.name))return false;
   old=t;
   return true;
  }
  if(types.size()>=MaxSavedTypes)return false;
  types.push_back(t);return true;
 }
 bool ensure(const std::string& id){
  if(!validId(id)||find(id)>=0||types.size()>=MaxSavedTypes)return false;
  types.push_back(placeholder(id));return true;
 }
};
}
