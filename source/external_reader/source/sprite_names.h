#pragma once
#include "game_archive.h"
namespace external { namespace assets {
// The live SHP reference is authoritative. Building parts then use the current
// theater, the generic G image, and the declared image; other climates are not
// substitutes for a missing snow/desert animation.
inline std::vector<std::string> spriteNames(std::string stem,const std::string&live,
 const std::string&extension,const std::string&letter,bool seasonal,bool newTheater,
 bool overlay=false,bool buildingPart=false){
 std::vector<std::string> out;
 auto add=[&](std::string s){s=name(s);if(safe(s)&&std::find(out.begin(),out.end(),s)==out.end())out.push_back(s);};
 auto variant=[&](char c){if(stem.size()>1){auto s=stem;s[1]=c;add(s+".SHP");}};
 if(!live.empty())add(live);stem=name(stem);if(stem.empty())return out;
 bool supports=std::string("GNCY").find(stem[0])!=std::string::npos;
 if(buildingPart){
  if((overlay||seasonal)&&!extension.empty())add(stem+"."+extension);
  if(newTheater||(!overlay&&supports)){if(!letter.empty())variant(letter[0]);variant('G');}
  add(stem+".SHP");return out;
 }
 // Preserve the legacy broad fallback for non-building resources.
 auto extensions=[&](){if(!extension.empty())add(stem+"."+extension);for(auto e:{"TEM","SNO","URB","LUN","DES","UBN"})add(stem+"."+e);};
 auto variants=[&](){if(!letter.empty())variant(letter[0]);for(char c:std::string("GTAUNDL"))variant(c);};
 if(overlay){if(!extension.empty())add(stem+"."+extension);}else if(seasonal)extensions();
 if(newTheater||(!overlay&&supports))variants();add(stem+".SHP");extensions();variants();
 return out;
}
}}
