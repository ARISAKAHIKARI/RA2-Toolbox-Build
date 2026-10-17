#pragma once
#include <cstdint>
#include <cstring>
#include <string>
namespace assist { namespace oreconfig {
constexpr int Limit=512;
struct Config {uint32_t customMiners=0,customRefineries=0;int minerCount=0,refineryCount=0;char miners[Limit][24]{},refineries[Limit][24]{};};
inline char upper(char c){return c>='a'&&c<='z'?char(c-'a'+'A'):c;}
inline bool equal(const char*a,const char*b){for(int i=0;i<24;++i){if(upper(a[i])!=upper(b[i]))return false;if(!a[i])return true;}return false;}
inline bool valid(const Config&c){if(c.customMiners>1||c.customRefineries>1||c.minerCount<0||c.minerCount>Limit||c.refineryCount<0||c.refineryCount>Limit)return false;
 for(int side=0;side<2;++side){int n=side?c.refineryCount:c.minerCount;auto ids=side?c.refineries:c.miners;for(int i=0;i<n;++i)if(!ids[i][0]||!memchr(ids[i],0,24))return false;}return true;}
inline bool allowed(const Config&c,bool miner,const char*id){if(!(miner?c.customMiners:c.customRefineries))return true;
 auto ids=miner?c.miners:c.refineries;int n=miner?c.minerCount:c.refineryCount;if(n<0||n>Limit)return false;for(int i=0;i<n;++i)if(equal(ids[i],id))return true;return false;}
inline bool parse(const std::wstring&text,char (&out)[Limit][24],int&count){
 char parsed[Limit][24]{};int n=0;std::string id;
 auto add=[&](){if(id.empty())return true;for(int i=0;i<n;++i)if(equal(parsed[i],id.c_str())){id.clear();return true;}if(n==Limit)return false;memcpy(parsed[n++],id.c_str(),id.size()+1);id.clear();return true;};
 for(wchar_t c:text){if(c==L','||c==L';'||c==L' '||c==L'\t'||c==L'\n'||c==L'\r'||c==L'，'||c==L'；'){if(!add())return false;continue;}
  if(!((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'_'||c==L'-')||id.size()==23)return false;id+=upper(char(c));}
 if(!add())return false;memcpy(out,parsed,sizeof parsed);count=n;return true;
}
}}
