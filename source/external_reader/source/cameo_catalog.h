#pragma once
#include "live_ini.h"
#include <set>
#include <functional>
namespace external { namespace assets {
class CameoCatalog {
 ArtINI rules_,art_;std::function<bool(bool,const std::string&,const std::string&,std::string&)> live_;uint64_t revision_=0;bool loaded_=false;
 static bool usable(const std::string&s){auto n=name(s);return !n.empty()&&n!="NONE"&&n!="<NONE>"&&safe(n);}
public:
 void live(std::function<bool(bool,const std::string&,const std::string&,std::string&)> read){live_=std::move(read);}
 void clear(){rules_.clear();art_.clear();revision_=0;loaded_=false;}
 template<class Load>void update(Load load,uint64_t revision){
  if(loaded_&&revision_==revision)return;clear();revision_=revision;std::vector<uint8_t> bytes;
  if(load("RULESMD.INI",bytes)||load("RULES.INI",bytes))rules_.parse(bytes);
  if(load("ARTMD.INI",bytes)||load("ART.INI",bytes))art_.parse(bytes);
  loaded_=true;
 }
 std::string resolve(const std::string&id,const std::string&nativeImage,const std::string&nativeCameo,bool elite=false)const{
  // A live type contains the result after includes, map and MOD overrides.
  // An explicit native None is authoritative; absent/unreadable fields fall back.
  auto current=name(nativeCameo);if(current=="NONE"||current=="<NONE>")return {};
  if(usable(current)){if(current.find('.')==current.npos)current+=".SHP";return current;}
  auto get=[&](bool rules,const std::string&s,const std::string&k){std::string out;if(live_&&live_(rules,s,k,out))return out;return (rules?rules_:art_).get(s,k);};
  std::vector<std::string> roots;if(usable(nativeImage))roots.push_back(nativeImage);
  auto image=get(true,id,"IMAGE");if(usable(image))roots.push_back(image);else roots.push_back(id);
  std::set<std::string> seen;
  for(auto section:roots)for(int hops=0;hops<8&&usable(section);++hops){
   section=name(section);if(!seen.insert(section).second)break;
   auto file=elite?get(false,section,"ALTCAMEO"):std::string{};if(file.empty())file=get(false,section,"CAMEO");
   if(!file.empty()){if(!usable(file))return {};file=name(file);if(file.find('.')==file.npos)file+=".SHP";return file;}
   section=get(false,section,"IMAGE");
  }
  if(!usable(nativeCameo))return {};auto file=name(nativeCameo);if(file.find('.')==file.npos)file+=".SHP";return file;
 }
};

// File fallback and native INI lookup share the same mapping for all icon users.
template<class Read>class RuntimeCameos {
 Read read_;CameoCatalog catalog_;LiveINI rules_,art_;
public:
 explicit RuntimeCameos(Read read):read_(read){}
 void clear(){catalog_.clear();rules_.clear();art_.clear();}
 template<class Load>std::string resolve(Load load,uint64_t revision,uint64_t generation,
  const std::string&id,const std::string&image,const std::string&cameo){
  catalog_.update(load,revision);
  catalog_.live([this,generation](bool rules,const std::string&s,const std::string&k,std::string&out){
   uint32_t object=LiveINI::ArtObject;
   if(rules&&!read_(LiveINI::RulesPointer,&object,4))return false;
   return (rules?rules_:art_).get(read_,object,generation,s,k,out);
  });
  return catalog_.resolve(id,image,cameo);
 }
};
}}
