#pragma once
#include "game_archive.h"
#include <windows.h>
namespace external { namespace assets {
class Files {
 std::wstring root;Directory directory;uint32_t last=0,epoch=0,process=0;
 uint64_t size(const std::string&nm)const{if(!safe(nm))return 0;WIN32_FILE_ATTRIBUTE_DATA a{};std::wstring p=root+std::wstring(nm.begin(),nm.end());if(!GetFileAttributesExW(p.c_str(),GetFileExInfoStandard,&a)||(a.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))return 0;return (uint64_t(a.nFileSizeHigh)<<32)|a.nFileSizeLow;}
 bool read(const std::string&nm,uint64_t offset,void*out,size_t n)const{
  if(!safe(nm)||n>8*1024*1024)return false;std::wstring p=root+std::wstring(nm.begin(),nm.end());HANDLE f=CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return false;
  LARGE_INTEGER pos;pos.QuadPart=offset;DWORD got=0;bool ok=SetFilePointerEx(f,pos,nullptr,FILE_BEGIN)&&ReadFile(f,out,DWORD(n),&got,nullptr)&&got==n;CloseHandle(f);return ok;
 }
public:
 void clear(){root.clear();directory.clear();last=epoch=process=0;}
 template<class Reader>void update(Reader&r,uint32_t pid,uint32_t match){
  uint32_t now=GetTickCount();if(pid==process&&match==epoch&&last&&uint32_t(now-last)<5000)return;
  if(pid!=process||match!=epoch)clear();process=pid;epoch=match;last=now;
  wchar_t path[32768]{};DWORD n=32768;if(!r.imagePath(path,&n)){clear();return;}root=path;size_t slash=root.find_last_of(L"\\/");if(slash==std::wstring::npos){clear();return;}root.resize(slash+1);
  std::vector<Slice> files;WIN32_FIND_DATAW info{};HANDLE search=FindFirstFileW((root+L"*.mix").c_str(),&info);if(search!=INVALID_HANDLE_VALUE){do{if(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;std::wstring w=info.cFileName;if(w.size()>127||std::any_of(w.begin(),w.end(),[](wchar_t c){return c>127;}))continue;files.push_back({std::string(w.begin(),w.end()),0,(uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow});}while(files.size()<512&&FindNextFileW(search,&info));FindClose(search);}
  auto memory=[&](uint32_t p,void*out,size_t n){return r.readFresh(p,out,n);};directory.build(std::move(files),indices(memory),[&](auto&nm,uint64_t at,void*out,size_t n){return read(nm,at,out,n);});
 }
 bool load(const std::string&nm,std::vector<uint8_t>&out){if(root.empty())return false;return directory.load(nm,out,[&](auto&file,uint64_t at,void*p,size_t n){return read(file,at,p,n);},size(nm));}
};
}}
