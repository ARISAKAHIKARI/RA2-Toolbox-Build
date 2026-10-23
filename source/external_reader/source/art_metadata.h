#pragma once
#include "game_archive.h"
#include <map>
#include <sstream>
namespace external { namespace assets {
// Small read-only INI view for art lookup. Values retain their original text;
// sections/keys are case independent like the original loader.
class ArtINI {
 std::map<std::string,std::map<std::string,std::string>> sections_;
 static std::string trim(std::string s){auto a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");return a==s.npos?std::string{}:s.substr(a,b-a+1);}
public:
 void clear(){sections_.clear();}
 void parse(const std::vector<uint8_t>&data){std::istringstream input(std::string(data.begin(),data.end()));std::string line,section;
  while(std::getline(input,line)){if(line.size()>16384)continue;auto comment=line.find(';');if(comment!=line.npos)line.resize(comment);line=trim(line);if(line.empty())continue;
   if(line.front()=='['&&line.back()==']'){section=name(trim(line.substr(1,line.size()-2)));continue;}
   auto equal=line.find('=');if(section.empty()||equal==line.npos)continue;auto key=name(trim(line.substr(0,equal)));if(!key.empty())sections_[section][key]=trim(line.substr(equal+1));
  }
 }
 std::string get(const std::string&s,const std::string&k)const{auto i=sections_.find(name(s));if(i==sections_.end())return {};auto j=i->second.find(name(k));return j==i->second.end()?std::string{}:j->second;}
 bool yes(const std::string&s,const std::string&k)const{auto v=name(get(s,k));return v=="YES"||v=="TRUE"||v=="1";}
};
}}
