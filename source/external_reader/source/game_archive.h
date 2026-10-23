#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <tuple>
#include "mix_crypto.h"
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
 static bool locateIn(const Archive&a,const std::string&nm,Slice&out){
  uint32_t id=hash(nm);auto it=std::lower_bound(a.entries.begin(),a.entries.end(),id,[](auto&e,uint32_t n){return e.id<n;});
  if(it==a.entries.end()||it->id!=id)return false;out={a.slice.file,a.slice.offset+a.start+it->offset,it->size};return true;
 }
 template<class Read>bool encrypted(Archive&a,Read read){
  if(a.slice.size<92)return false;uint8_t source[80],key[56],first[8];
  if(!read(a.slice.file,a.slice.offset+4,source,80)||!read(a.slice.file,a.slice.offset+84,first,8))return false;
  mixcrypto::key(source,key);mixcrypto::Blowfish fish(key);fish.decrypt(first,8);
  unsigned count=unsigned(first[0])|(unsigned(first[1])<<8);if(!count)return false;
  unsigned size=(6+count*12+7)&~7u;a.start=84+size;if(a.start>a.slice.size)return false;
  std::vector<uint8_t> bytes(size);if(!read(a.slice.file,a.slice.offset+84,bytes.data(),size))return false;fish.decrypt(bytes.data(),size);
  a.entries.resize(count);memcpy(a.entries.data(),bytes.data()+6,count*12);return true;
 }
 template<class Read>void customNames(const Archive&a,std::vector<std::string>&names,Read read)const{
  Slice db;if(!locateIn(a,"LOCAL MIX DATABASE.DAT",db)||db.size<52||db.size>1024*1024)return;
  std::vector<uint8_t> bytes(size_t(db.size));if(!read(db.file,db.offset,bytes.data(),bytes.size()))return;
  const uint8_t signature[]={0x58,0x43,0x43,0x20,0x62,0x79,0x20,0x4f,0x6c,0x61,0x66,0x20,0x76,0x61,0x6e,0x20,0x64,0x65,0x72,0x20,0x53,0x70,0x65,0x6b,0x1a,0x04,0x17,0x27,0x10,0x19,0x80,0};
  if(memcmp(bytes.data(),signature,sizeof signature))return;
  unsigned count=word(bytes.data()+48);if(count>65535)return;size_t at=52;std::vector<std::string> found;
  for(unsigned i=0;i<count;++i){size_t start=at;while(at<bytes.size()&&bytes[at])++at;if(at==bytes.size())return;
   if(at-start<128){std::string nm(reinterpret_cast<char*>(bytes.data()+start),at-start);nm=name(nm);if(safe(nm)&&nm.size()>4&&nm.compare(nm.size()-4,4,".MIX")==0)found.push_back(std::move(nm));}++at;
  }
  names.insert(names.end(),found.begin(),found.end());
 }
public:
 void clear(){archives.clear();live.clear();}
 template<class Read>bool mount(const std::string&nm,Slice file,int rank,Read read){
  uint8_t header[10]{};if(!safe(nm)||file.size<10||!read(file.file,file.offset,header,10))return false;
  unsigned flags=word(header);bool classic=(flags&65535)!=0,encrypted=!classic&&(flags&0x20000);Archive a;a.name=name(nm);a.slice=file;a.priority=rank;
  if(encrypted){
   if(!this->encrypted(a,read)){
    // Compatibility fallback for custom game-decoded directories. Never use
    // an ambiguous basename shared by several live archive nodes.
    const LiveIndex*index=nullptr;for(const auto&i:live)if(i.name==a.name){if(index)return false;index=&i;}
    if(!index||index->entries.empty()||index->entries.size()>65535)return false;
    a.entries=index->entries;a.start=84+((6+a.entries.size()*12+7)&~size_t(7));
   }
  }
  else{unsigned at=classic?0:4,count=header[at]|unsigned(header[at+1])<<8;if(!count||count>65535)return false;a.start=at+6+count*12;if(a.start>file.size)return false;a.entries.resize(count);if(!read(file.file,file.offset+at+6,a.entries.data(),count*12))return false;}
  if(a.start>file.size)return false;for(auto&e:a.entries)if(uint64_t(a.start)+e.offset+e.size>file.size)return false;
  std::sort(a.entries.begin(),a.entries.end(),[](auto&a,auto&b){return a.id<b.id;});archives.push_back(std::move(a));return true;
 }
 bool locate(std::string nm,Slice&out)const{for(const auto&a:archives)if(locateIn(a,nm,out))return true;return false;}
 template<class Read>void build(std::vector<Slice> files,std::vector<LiveIndex> decoded,Read read){
  clear();archives.reserve(512);live=std::move(decoded);std::sort(files.begin(),files.end(),[](auto&a,auto&b){int x=priority(name(a.file)),y=priority(name(b.file));return x==y?name(a.file)>name(b.file):x>y;});
  using Identity=std::tuple<std::string,uint64_t,uint64_t>;std::set<Identity> mounted;
  auto attach=[&](const std::string&nm,const Slice&s,int rank){auto id=Identity{name(s.file),s.offset,s.size};if(archives.size()>=512||!mounted.insert(id).second)return false;return mount(nm,s,rank,read);};
  for(const auto&f:files)attach(name(f.file),f,priority(name(f.file)));
  auto standard=standardArchives();for(const auto&i:live)standard.push_back(i.name);
  size_t first=0,end=archives.size();
  for(int depth=0;depth<4&&first<end;++depth){
   for(size_t i=first;i<end;++i){const auto&a=archives[i];auto names=standard;customNames(a,names,read);
    std::sort(names.begin(),names.end());names.erase(std::unique(names.begin(),names.end()),names.end());
    for(const auto&nm:names){Slice s;if(locateIn(a,nm,s))attach(nm,s,a.priority);}
   }
   first=end;end=archives.size();
  }
  // A child is identified by its file extent, not its basename. An expansion's
  // LOCALMD.MIX must override individual files without hiding base-only files
  // in RA2MD.MIX's different LOCALMD.MIX.
  std::stable_sort(archives.begin(),archives.end(),[](auto&a,auto&b){return a.priority>b.priority;});
 }
 template<class Read>bool load(const std::string&nm,std::vector<uint8_t>&out,Read read,uint64_t looseSize){
  out.clear();if(!safe(nm))return false;Slice s;if(looseSize)s={nm,0,looseSize};else if(!locate(nm,s))return false;
  if(!s.size||s.size>8*1024*1024)return false;std::vector<uint8_t> bytes(size_t(s.size),0);if(!read(s.file,s.offset,bytes.data(),bytes.size()))return false;out=std::move(bytes);return true;
 }
 size_t count()const{return archives.size();}
};
}}
