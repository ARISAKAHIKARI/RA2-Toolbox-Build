#pragma once
// Ra2Tool README-compatible technology lists. Independent implementation:
// upstream game hook implementation was NOT available to audit or copy.
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <limits>

namespace techprofile {
// YR sidebar categories: 7=buildings, 16=infantry, 40=vehicles.
// A value's number is its index in that type array (not TechLevel).
inline bool knownKind(int kind) {return kind==7||kind==16||kind==40||kind==3;}
struct Lists {
 std::map<int,std::set<int>> types;
 bool contains(int kind,int index) const {
  auto it=types.find(kind);
  return index>=0&&it!=types.end()&&it->second.count(index)!=0;
 }
};
inline Lists preset(){
 Lists l;
 l.types[7]={18,47,56,64,71,72,73,183};
 l.types[16]={8,16};
 l.types[40]={0,26,54,16,27,2,14,15,34,37,38};
 return l;
}
inline std::set<int> parseIndexes(const std::string& text){
 std::set<int> result;
 std::string sanitized=text;
 for(char& c:sanitized)if(c==','||c==';'||c=='\t')c=' ';
 std::istringstream input(sanitized);
 std::string word;
 while(input>>word){
  if(word.empty())continue;
  size_t start=word[0]=='+'?1:0;
  if(start==word.size())continue;
  bool digits=true;
  for(size_t n=start;n<word.size();++n)if(word[n]<'0'||word[n]>'9'){digits=false;break;}
  if(!digits)continue;
  try{long long v=std::stoll(word);if(v>=0&&v<=4095)result.insert(int(v));}catch(...){}
 }
 return result;
}
inline bool allowed(const Lists& ra2,int profile,int kind,int index,bool legacyDefault){
 if(profile==0){static const Lists p=preset();return p.contains(kind,index);} // Ra2Tool built-in defaults
 if(profile==1)return ra2.contains(kind,index); // [tech] CustomTech=1
 if(profile==2)return legacyDefault; // Legacy all/none with per-ID exceptions
 return false;
}
}
