#pragma once
#include "game_archive.h"
#include <windows.h>
namespace external { namespace assets {
class Files {
 std::wstring root;Directory directory;uint32_t last=0,epoch=0,process=0;uint64_t fingerprint=0;
 uint64_t size(const std::string&nm)const{if(!safe(nm))return 0;WIN32_FILE_ATTRIBUTE_DATA a{};std::wstring p=root+std::wstring(nm.begin(),nm.end());if(!GetFileAttributesExW(p.c_str(),GetFileExInfoStandard,&a)||(a.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))return 0;return (uint64_t(a.nFileSizeHigh)<<32)|a.nFileSizeLow;}
 bool read(const std::string&nm,uint64_t offset,void*out,size_t n)const{
  if(!safe(nm)||n>8*1024*1024)return false;std::wstring p=root+std::wstring(nm.begin(),nm.end());HANDLE f=CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return false;
  LARGE_INTEGER pos;pos.QuadPart=offset;DWORD got=0;bool ok=SetFilePointerEx(f,pos,nullptr,FILE_BEGIN)&&ReadFile(f,out,DWORD(n),&got,nullptr)&&got==n;CloseHandle(f);return ok;
 }
public:
 void clear(){root.clear();directory.clear();last=epoch=process=0;fingerprint=0;}
 template<class Reader>void update(Reader&r,uint32_t pid,uint32_t match){
  uint32_t now=GetTickCount();if(pid==process&&match==epoch&&last&&uint32_t(now-last)<5000)return;
  if(pid!=process||match!=epoch)clear();process=pid;epoch=match;last=now;
  wchar_t path[32768]{};DWORD n=32768;if(!r.imagePath(path,&n)){clear();return;}root=path;size_t slash=root.find_last_of(L"\\/");if(slash==std::wstring::npos){clear();return;}root.resize(slash+1);
  struct Stamp {Slice slice;uint64_t time;};std::vector<Stamp> stamps;
  WIN32_FIND_DATAW info{};HANDLE search=FindFirstFileW((root+L"*.mix").c_str(),&info);if(search!=INVALID_HANDLE_VALUE){do{if(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;std::wstring w=info.cFileName;if(w.size()>127||std::any_of(w.begin(),w.end(),[](wchar_t c){return c>127;}))continue;stamps.push_back({{std::string(w.begin(),w.end()),0,(uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow},(uint64_t(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime});}while(stamps.size()<512&&FindNextFileW(search,&info));FindClose(search);}
  std::sort(stamps.begin(),stamps.end(),[](const auto&a,const auto&b){return a.slice.file<b.slice.file;});
  auto memory=[&](uint32_t p,void*out,size_t n){return r.readFresh(p,out,n);};auto decoded=indices(memory);
  // Re-scan metadata, but do not repeat RSA/Blowfish and index reads every five
  // seconds while the installation and game's decoded directory are unchanged.
  uint64_t next=14695981039346656037ull;auto fold=[&](const void*p,size_t n){const auto*b=static_cast<const uint8_t*>(p);for(size_t i=0;i<n;++i){next^=b[i];next*=1099511628211ull;}};
  std::vector<Slice> files;for(const auto&s:stamps){fold(s.slice.file.c_str(),s.slice.file.size()+1);fold(&s.slice.size,sizeof s.slice.size);fold(&s.time,sizeof s.time);files.push_back(s.slice);}
  for(const auto&i:decoded){fold(i.name.c_str(),i.name.size()+1);uint32_t count=uint32_t(i.entries.size());fold(&count,sizeof count);fold(i.entries.data(),i.entries.size()*sizeof(Entry));}
  if(next==fingerprint&&directory.count())return;
  directory.build(std::move(files),std::move(decoded),[&](auto&nm,uint64_t at,void*out,size_t n){return read(nm,at,out,n);});fingerprint=next;
 }
 size_t count()const{return directory.count();}
 bool load(const std::string&nm,std::vector<uint8_t>&out){if(root.empty())return false;return directory.load(nm,out,[&](auto&file,uint64_t at,void*p,size_t n){return read(file,at,p,n);},size(nm));}
};
}}
