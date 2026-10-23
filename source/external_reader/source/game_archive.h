#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <set>
namespace external { namespace assets {
inline std::string name(std::string s){for(auto&c:s)if(c>='a'&&c<='z')c-=32;return s;}
inline bool safe(const std::string&s){return !s.empty()&&s.size()<128&&s.find_first_of("/\\:")==std::string::npos&&s!="."&&s!="..";}
// MIX node names may include the game's installation path. Use only the
// basename as an archive identity; disk access remains confined to game root.
inline std::string archiveName(std::string s){auto p=s.find_last_of("/\\");if(p!=std::string::npos)s.erase(0,p+1);return safe(s)?name(s):std::string{};}
inline std::vector<std::string> standardArchives(){
 std::vector<std::string> out={"RA2MD.MIX","RA2.MIX","LANGMD.MIX","LANGUAGE.MIX","CACHEMD.MIX","CACHE.MIX","LOCALMD.MIX","LOCAL.MIX","CONQMD.MIX","CONQUER.MIX","ISOGENMD.MIX","ISOGEN.MIX"};
 const char*theaters[][4]={{"TEMPERAT","ISOTEMP","ISOTEM","TEM"},{"SNOW","ISOSNOW","ISOSNO","SNO"},{"URBAN","ISOURB","ISOURB","URB"},{"DESERT","ISODES","ISODES","DES"},{"URBANN","ISOUBN","ISOUBN","UBN"},{"LUNAR","ISOLUN","ISOLUN","LUN"}};
 for(auto&t:theaters){for(int i:{0,1,3})out.emplace_back(std::string(t[i])+".MIX");for(int i:{0,2})out.emplace_back(std::string(t[i])+"MD.MIX");}return out;
}
inline uint32_t word(const uint8_t*p){uint32_t n;memcpy(&n,p,4);return n;}
inline uint32_t hash(std::string s){s=name(s);size_t n=s.size(),r=n%4;if(r){s.push_back(char(r));while(s.size()%4)s.push_back(s[n-r]);}uint32_t c=~0u;for(uint8_t b:s){c^=b;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));}return ~c;}
struct Entry {uint32_t id=0,offset=0,size=0;};
struct LiveIndex {std::string name;std::vector<Entry> entries;};
struct Slice {std::string file;uint64_t offset=0,size=0;};
struct Archive {std::string name;Slice slice;uint32_t start=0;int priority=0;std::vector<Entry> entries;};
// The encrypted MIX directory is already decoded by the game. Only that
// bounded index is sampled; resource bytes always come from the original file.
// No call into the game loader and no dependency on loaded SHP/VXL/TMP data.
template<class Read>std::vector<LiveIndex> indices(Read read){
 auto get=[&](uint32_t p){uint32_t n=0;read(p,&n,4);return n;};std::vector<LiveIndex> out;std::set<uint32_t> seen;
 uint32_t p=get(0xABEFD8+8);for(int n=0;n<512&&p>=0x10000&&seen.insert(p).second;++n){
  uint32_t next=get(p+4),filename=get(p+12),count=get(p+20),entries=get(p+32);if(!next)break;
  char text[1024]{};bool ok=false;for(int k=0;k<1023&&filename>=0x10000;++k){if(!read(filename+k,text+k,1))break;if(!text[k]){ok=true;break;}}
  auto nm=ok?archiveName(text):std::string{};if(!nm.empty()&&count&&count<=65535&&entries>=0x10000){LiveIndex index;index.name=nm;index.entries.resize(count);if(read(entries,index.entries.data(),count*sizeof(Entry))&&get(p+32)==entries&&get(p+20)==count)out.push_back(std::move(index));}p=next;
 }return out;
}
inline int priority(const std::string&n){int serial=0;if(n.size()>8)for(size_t i=8;i<n.size()&&i<11&&n[i]>='0'&&n[i]<='9';++i)serial=serial*10+n[i]-'0';if(n.rfind("EXPANDMD",0)==0)return 50000+serial;if(n.rfind("ECACHEMD",0)==0)return 40000+serial;if(n=="RA2MD.MIX")return 30000;if(n=="RA2.MIX")return 10000;return 20000;}
class Directory {
 std::vector<Archive> archives;std::vector<LiveIndex> live;
public:
 void clear(){archives.clear();live.clear();}
 template<class Read>bool mount(const std::string&nm,Slice file,int rank,Read read){
  uint8_t header[10]{};if(!safe(nm)||file.size<10||!read(file.file,file.offset,header,10))return false;
  unsigned flags=word(header);bool classic=(flags&65535)!=0,encrypted=!classic&&(flags&0x20000);Archive a;a.name=name(nm);a.slice=file;a.priority=rank;
  if(encrypted){auto it=std::find_if(live.begin(),live.end(),[&](const auto&i){return i.name==a.name;});if(it==live.end()||it->entries.empty())return false;a.entries=it->entries;a.start=84+((6+a.entries.size()*12+7)&~size_t(7));}
  else{unsigned at=classic?0:4,count=header[at]|unsigned(header[at+1])<<8;if(!count||count>65535)return false;a.start=at+6+count*12;if(a.start>file.size)return false;a.entries.resize(count);if(!read(file.file,file.offset+at+6,a.entries.data(),count*12))return false;}
  if(a.start>file.size)return false;for(auto&e:a.entries)if(uint64_t(a.start)+e.offset+e.size>file.size)return false;
  std::sort(a.entries.begin(),a.entries.end(),[](auto&a,auto&b){return a.id<b.id;});archives.push_back(std::move(a));return true;
 }
 bool locate(std::string nm,Slice&out)const{uint32_t id=hash(nm);for(const auto&a:archives){auto it=std::lower_bound(a.entries.begin(),a.entries.end(),id,[](auto&e,uint32_t n){return e.id<n;});if(it!=a.entries.end()&&it->id==id){out={a.slice.file,a.slice.offset+a.start+it->offset,it->size};return true;}}return false;}
 template<class Read>void build(std::vector<Slice> files,std::vector<LiveIndex> decoded,Read read){
  clear();live=std::move(decoded);std::sort(files.begin(),files.end(),[](auto&a,auto&b){int x=priority(name(a.file)),y=priority(name(b.file));return x==y?name(a.file)>name(b.file):x>y;});
  std::set<std::string> mounted;for(const auto&f:files)if(archives.size()<512&&mount(name(f.file),f,priority(name(f.file)),read))mounted.insert(name(f.file));
  // Discover standard nested packs even before they acquire live cache nodes.
  // Live names additionally cover MOD archives; encrypted ones still require
  // a validated decoded index. No inference of encrypted directory contents.
  auto names=standardArchives();for(const auto&i:live)names.push_back(i.name);
  // Loaded nested MIX names cover theater, cache, cameo, local and MOD packs.
  // Expand children retain their parent's priority above base-game resources.
  for(int depth=0;depth<4;++depth){bool changed=false;for(const auto&nm:names){if(mounted.count(nm)||archives.size()>=512)continue;Slice s;if(!locate(nm,s))continue;int rank=priority(name(s.file));if(mount(nm,s,rank,read)){mounted.insert(nm);changed=true;}}
   std::stable_sort(archives.begin(),archives.end(),[](auto&a,auto&b){return a.priority>b.priority;});if(!changed)break;
  }
 }
 template<class Read>bool load(const std::string&nm,std::vector<uint8_t>&out,Read read,uint64_t looseSize){
  out.clear();if(!safe(nm))return false;Slice s;if(looseSize)s={nm,0,looseSize};else if(!locate(nm,s))return false;
  if(!s.size||s.size>8*1024*1024)return false;std::vector<uint8_t> bytes(size_t(s.size),0);if(!read(s.file,s.offset,bytes.data(),bytes.size()))return false;out=std::move(bytes);return true;
 }
 size_t count()const{return archives.size();}
};
}}
