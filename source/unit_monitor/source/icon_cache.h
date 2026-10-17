#pragma once
#include "core.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <cwctype>
#include <vector>
namespace um {
namespace iconcache {
inline uint64_t hash(const void*p,size_t n,uint64_t h=14695981039346656037ull){auto b=static_cast<const uint8_t*>(p);while(n--){h^=*b++;h*=1099511628211ull;}return h;}
struct Record {int category;char id[24],cameo[25];uint8_t icon[IW*IH];uint32_t palette[256];};
struct Header {uint32_t magic=0x55494332,version=1,count=0,bytes=0;uint64_t scope=0,checksum=0;};
inline std::string key(const Record&r){return std::to_string(r.category)+":"+r.id+":"+r.cameo;}
inline std::string key(const Type&t){return std::to_string(t.category)+":"+t.id+":"+t.cameo;}
inline bool valid(const Record&r){return r.category>=0&&r.category<4&&r.id[0]&&r.cameo[0]&&memchr(r.id,0,sizeof r.id)&&memchr(r.cameo,0,sizeof r.cameo);}
inline std::vector<Record> read(const std::filesystem::path&p,uint64_t scope){
 std::ifstream f(p,std::ios::binary);Header h{};if(!f.read(reinterpret_cast<char*>(&h),sizeof h)||h.magic!=0x55494332||h.version!=1||h.scope!=scope||h.count>MaxTypes||h.bytes!=h.count*sizeof(Record))return {};
 std::vector<Record> records(h.count);if(!f.read(reinterpret_cast<char*>(records.data()),h.bytes)||f.peek()!=EOF||hash(records.data(),h.bytes)!=h.checksum)return {};
 for(auto&r:records)if(!valid(r))return {};return records;
}
inline bool write(const std::filesystem::path&p,uint64_t scope,const std::vector<Record>&records){
 if(records.size()>MaxTypes)return false;Header h;h.scope=scope;h.count=uint32_t(records.size());h.bytes=h.count*sizeof(Record);h.checksum=hash(records.data(),h.bytes);
 std::error_code ec;std::filesystem::create_directories(p.parent_path(),ec);if(ec)return false;
 auto temp=p;temp+=L".tmp";{std::ofstream f(temp,std::ios::binary|std::ios::trunc);if(!f.write(reinterpret_cast<const char*>(&h),sizeof h)||!f.write(reinterpret_cast<const char*>(records.data()),h.bytes)||!f.flush())return false;}
#ifdef _WIN32
 return MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
 std::filesystem::rename(temp,p,ec);return !ec;
#endif
}
// Metadata for large resource archives; content hashes for map/rules overrides.
// Work runs off the sampling/UI thread and never reads game memory.
inline uint64_t scope(const std::filesystem::path&exe){
 std::error_code ec;auto base=exe.parent_path();std::vector<std::filesystem::path> files;
 std::filesystem::recursive_directory_iterator it(base,std::filesystem::directory_options::skip_permission_denied,ec),end;
 size_t seen=0;for(;!ec&&it!=end;it.increment(ec)){if(++seen>30000)return 0;if(it.depth()>2){it.disable_recursion_pending();continue;}if(!it->is_regular_file(ec))continue;
  auto ext=it->path().extension().wstring(),name=it->path().filename().wstring();for(auto&c:ext)c=towlower(c);for(auto&c:name)c=towlower(c);
  bool ini=ext==L".ini"&&name!=L"spawn.ini";
  if(ini||ext==L".mix"||ext==L".shp"||ext==L".pal"||ext==L".map"||ext==L".mpr"||ext==L".yrm"||ext==L".dll"||it->path()==exe)files.push_back(it->path());
 }if(ec)return 0;std::sort(files.begin(),files.end());auto full=exe.wstring();uint64_t h=hash(full.data(),full.size()*sizeof(wchar_t));
 for(auto&p:files){auto n=p.lexically_relative(base).wstring();h=hash(n.data(),n.size()*sizeof(wchar_t),h);auto size=std::filesystem::file_size(p,ec);if(ec)return 0;h=hash(&size,sizeof size,h);
  if(size<=8*1024*1024){std::ifstream f(p,std::ios::binary);if(!f)return 0;char b[65536];while(f){f.read(b,sizeof b);h=hash(b,size_t(f.gcount()),h);}if(!f.eof())return 0;}
  else{auto stamp=std::filesystem::last_write_time(p,ec).time_since_epoch().count();if(ec)return 0;h=hash(&stamp,sizeof stamp,h);}
 }return h?h:1;
}
}
class IconCache {
 std::mutex mutex;std::condition_variable cv;std::thread worker;bool stop=false,contextPending=false,dirty=false,ready=false;uint64_t generation=0,scopeHash=0;
 std::filesystem::path exe,directory,file;std::map<std::string,iconcache::Record> entries;
 void run(){std::unique_lock<std::mutex> lock(mutex);for(;;){cv.wait_for(lock,std::chrono::milliseconds(500),[&]{return stop||contextPending;});
   if(contextPending){auto x=exe,dir=directory;auto gen=generation;contextPending=false;lock.unlock();auto h=iconcache::scope(x);auto p=dir/(std::to_string(h)+".bin");auto records=h?iconcache::read(p,h):std::vector<iconcache::Record>{};lock.lock();
    if(gen==generation){scopeHash=h;file=p;entries.clear();for(auto&r:records)entries[iconcache::key(r)]=r;ready=h!=0;loaded=int(entries.size());}continue;}
   if(dirty&&ready){std::vector<iconcache::Record> records;for(auto&e:entries)records.push_back(e.second);auto p=file;auto h=scopeHash;dirty=false;lock.unlock();bool ok=iconcache::write(p,h,records);lock.lock();writeFailed=!ok;}
   if(stop)break;
  }}
public:
 std::atomic<int> loaded{0};std::atomic<bool> writeFailed{false};
 ~IconCache(){if(worker.joinable()){{std::lock_guard<std::mutex> l(mutex);stop=true;}cv.notify_one();worker.join();}}
 void rescope(){std::lock_guard<std::mutex> l(mutex);++generation;ready=false;contextPending=true;cv.notify_one();}
 void open(const std::filesystem::path&game,const std::filesystem::path&folder){std::lock_guard<std::mutex> l(mutex);++generation;exe=game;directory=folder;entries.clear();ready=false;dirty=false;loaded=0;contextPending=true;if(!worker.joinable())worker=std::thread([this]{run();});cv.notify_one();}
 bool restorePalette(const Type&t,Snapshot&s){std::lock_guard<std::mutex> l(mutex);if(!ready)return false;auto it=entries.find(iconcache::key(t));if(it==entries.end())return false;
  memcpy(s.palette,it->second.palette,sizeof s.palette);s.paletteReady=1;return true;}
 bool restore(Type&t,Snapshot&s){std::lock_guard<std::mutex> l(mutex);if(!ready)return false;auto it=entries.find(iconcache::key(t));if(it==entries.end())return false;
  memcpy(t.icon,it->second.icon,sizeof t.icon);t.iconReady=2;t.iconCached=1;if(!s.paletteReady){memcpy(s.palette,it->second.palette,sizeof s.palette);s.paletteReady=1;}return true;}
 bool remember(const Type&t,const Snapshot&s){if(!s.paletteReady||t.iconReady!=1)return false;std::lock_guard<std::mutex> l(mutex);if(!ready)return false;
  auto k=iconcache::key(t);if(entries.size()>=MaxTypes&&!entries.count(k))return false;iconcache::Record r{};r.category=t.category;memcpy(r.id,t.id,sizeof r.id);memcpy(r.cameo,t.cameo,sizeof r.cameo);memcpy(r.icon,t.icon,sizeof r.icon);memcpy(r.palette,s.palette,sizeof r.palette);
  if(!iconcache::valid(r))return false;auto it=entries.find(k);if(it==entries.end()||memcmp(&it->second,&r,sizeof r)){entries[k]=r;dirty=true;}return true;}
};
}
