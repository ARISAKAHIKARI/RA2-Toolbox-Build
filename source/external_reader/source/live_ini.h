#pragma once
#include "art_metadata.h"
#include <cstring>
#include <utility>
namespace external { namespace assets {
// YR 1.001 CCINI layout, derived from YRpp d9795546 (CCINIClass,
// GenericList and IndexClass). Read-only; unsupported layouts fail closed.
// A live INI is not necessarily the final map/extension-modified type state.
class LiveINI {
 struct Index {uint32_t table=0;int32_t count=0,capacity=0;};
 struct Value {bool found=false;std::string text;};
 uint32_t root_=0;uint64_t generation_=0;Index rootIndex_{};bool indexed_=false;
 bool boundedPresentation_=false,indexFailed_=false;
 uint64_t indexBuilds_=0,indexFailures_=0,indexFailureReuses_=0;
 std::map<std::string,uint32_t> sections_;
 std::map<std::pair<std::string,std::string>,Value> values_;
 static bool range(uint32_t p,size_t n){return p>=0x10000&&uint64_t(p)+n<=0x80000000ull;}
 static bool same(const Index&a,const Index&b){return a.table==b.table&&a.count==b.count&&a.capacity==b.capacity;}
 template<class Read>static bool index(Read&read,uint32_t p,Index&out){
  if(!range(p,12)||!read(p,&out,12)||out.count<0||out.count>16384||out.capacity<out.count||out.capacity>65536)return false;
  return !out.count||range(out.table,size_t(out.count)*8);
 }
 template<class Read>static bool string(Read&read,uint32_t p,size_t limit,std::string&out){
  out.clear();if(!range(p,1))return false;
  for(size_t off=0;off<limit;){
   size_t n=std::min<size_t>(64,std::min(limit-off,size_t(4096-((p+off)&4095))));char b[64];
   if(!range(p+uint32_t(off),n))return false;
   if(!read(p+uint32_t(off),b,n)){n=1;if(!read(p+uint32_t(off),b,1))return false;}
   for(size_t i=0;i<n;++i){if(!b[i])return true;out.push_back(b[i]);}off+=n;
  }out.clear();return false;
 }
 template<class Read>static bool table(Read&read,uint32_t at,Index&header,std::vector<uint32_t>&data){
  if(!index(read,at,header))return false;data.resize(size_t(header.count)*2);
  if(!data.empty()&&!read(header.table,data.data(),data.size()*4))return false;
  Index after;return index(read,at,after)&&same(header,after);
 }
 template<class Read>static bool root(Read&read,uint32_t object,Index&header){
  uint32_t vt=0;return range(object,0x58)&&read(object,&vt,4)&&vt==0x7E1AF4&&index(read,object+0x28,header);
 }
 template<class Read>bool unchanged(Read&read,uint32_t object,const Index&header){
  Index after;return root(read,object,after)&&same(header,after);
 }
public:
 static constexpr uint32_t RulesPointer=0x887048,ArtObject=0x887180;
 struct IndexDiagnostics {uint64_t builds=0,failures=0,failureReuses=0;};
 IndexDiagnostics indexDiagnostics()const{return {indexBuilds_,indexFailures_,indexFailureReuses_};}
 // Opt-in only for a bounded render presentation. Every query still probes
 // the live root/index; a rejected full-section bootstrap is attempted once
 // for that exact identity during this pass, then retried on the next pass.
 // Other users retain the existing immediate retry after any failed lookup.
 void beginPresentation(){boundedPresentation_=true;indexFailed_=false;}
 void clear(){root_=0;generation_=0;rootIndex_={};indexed_=false;indexFailed_=false;sections_.clear();values_.clear();}
 template<class Read>bool get(Read read,uint32_t object,uint64_t generation,const std::string&section,const std::string&key,std::string&out){
  out.clear();if(section.empty()||key.empty()||section.size()>127||key.size()>127)return false;
  Index header;if(!root(read,object,header)){clear();return false;}
  if(object!=root_||generation!=generation_||!same(header,rootIndex_)){clear();root_=object;generation_=generation;rootIndex_=header;}
  if(!indexed_){
   if(boundedPresentation_&&indexFailed_){++indexFailureReuses_;return false;}
   ++indexBuilds_;
   struct Attempt {bool&failed;uint64_t&failures;bool completed=false;
    ~Attempt(){if(!completed){failed=true;++failures;}}
   } attempt{indexFailed_,indexFailures_};
   std::vector<uint32_t> entries;Index captured;if(!table(read,object+0x28,captured,entries)||!same(header,captured))return false;
   std::map<std::string,uint32_t> next;
   for(size_t i=1;i<entries.size();i+=2){uint32_t p=entries[i],np=0;std::string s;
    if(!range(p,0x40)||!read(p+0xC,&np,4)||!string(read,np,128,s)||s.empty())return false;
    if(!next.emplace(name(s),p).second)return false;
   }
   if(!unchanged(read,object,header))return false;sections_=std::move(next);indexed_=true;attempt.completed=true;
  }
  auto query=std::make_pair(name(section),name(key));auto found=values_.find(query);
  if(found!=values_.end()){if(!unchanged(read,object,header))return false;out=found->second.text;return found->second.found;}
  auto si=sections_.find(query.first);if(si==sections_.end())return false;
  uint32_t sectionPtr=si->second;std::vector<uint32_t> entries;Index captured;
  if(!table(read,sectionPtr+0x2C,captured,entries))return false;
  Value result;
  for(size_t i=1;i<entries.size();i+=2){uint32_t p=entries[i],kv[2]{};std::string k;
   if(!range(p,0x14)||!read(p+0xC,kv,8)||!string(read,kv[0],128,k))return false;
   if(name(k)!=query.second)continue;
   if(result.found||!string(read,kv[1],4096,result.text))return false;
   auto a=result.text.find_first_not_of(" \t\r\n"),b=result.text.find_last_not_of(" \t\r\n");
   result.text=a==result.text.npos?std::string{}:result.text.substr(a,b-a+1);result.found=true;
  }
  Index after;uint32_t namePointer=0;std::string sectionName;
  if(!index(read,sectionPtr+0x2C,after)||!same(captured,after)||!unchanged(read,object,header)||
     !read(sectionPtr+0xC,&namePointer,4)||!string(read,namePointer,128,sectionName)||name(sectionName)!=query.first)return false;
  if(values_.size()>=8192)values_.clear();values_[query]=result;out=result.text;return result.found;
 }
};
// File fallback is used only when the live key is unavailable. An explicitly
// empty live value remains empty instead of reviving a stale value on disk.
template<class Read>class EffectiveINI {
 Read read_;uint32_t address_;bool indirect_;uint64_t generation_=0;ArtINI disk_;LiveINI live_;
public:
 EffectiveINI(Read read,uint32_t address,bool indirect=false):read_(read),address_(address),indirect_(indirect){}
 void clear(){disk_.clear();live_.clear();}
 void parse(const std::vector<uint8_t>&bytes){disk_.parse(bytes);}
 void generation(uint64_t n){generation_=n;}
 void beginPresentation(){live_.beginPresentation();}
 LiveINI::IndexDiagnostics indexDiagnostics()const{return live_.indexDiagnostics();}
 bool live(const std::string&s,const std::string&k,std::string&out){
  uint32_t object=address_;if(indirect_&&!read_(address_,&object,4))return false;
  return live_.get(read_,object,generation_,s,k,out);
 }
 std::string get(const std::string&s,const std::string&k){std::string out;return live(s,k,out)?out:disk_.get(s,k);}
 bool yes(const std::string&s,const std::string&k){auto v=name(get(s,k));return v=="YES"||v=="TRUE"||v=="1";}
};
}}
