#pragma once
#include <windows.h>
#include <cstdint>
#include <cwchar>
#include <string>
#include <vector>

namespace external { namespace filedescription {
struct Translation {uint16_t language,codepage;};
// Query all advertised translations. A game can place its English description
// in a non-English resource table; the current UI language must not hide it.
template<class Query> std::wstring readDescriptions(Query query){
 std::vector<uint32_t> translations;
 const void* raw=nullptr;unsigned bytes=0;
 if(query(L"\\VarFileInfo\\Translation",&raw,&bytes)&&raw&&bytes>=sizeof(Translation)&&bytes<=4096){
  const auto* items=static_cast<const Translation*>(raw);
  for(unsigned i=0;i<bytes/sizeof(Translation);++i)
   translations.push_back(uint32_t(items[i].language)<<16|items[i].codepage);
 }
 // Older resources sometimes omit Translation but still have an English table.
 for(uint32_t fallback:{0x040904b0u,0x040904e4u,0x000004b0u}){
  bool found=false;for(uint32_t value:translations)if(value==fallback)found=true;
  if(!found)translations.push_back(fallback);
 }
 std::wstring result;
 for(uint32_t value:translations){
  wchar_t key[80]{};std::swprintf(key,80,L"\\StringFileInfo\\%04x%04x\\FileDescription",unsigned(value>>16),unsigned(value&0xffff));
  const void* text=nullptr;unsigned count=0;
  if(!query(key,&text,&count)||!text||!count||count>4096)continue;
  const auto* chars=static_cast<const wchar_t*>(text);size_t length=0;
  while(length<count&&chars[length])++length;
  if(!length)continue;
  const std::wstring description(chars,length);
  if(result==description||result.find(L"\n"+description+L"\n")!=std::wstring::npos)continue;
  if(result.size()+description.size()+1>8192)break;
  if(!result.empty())result+=L"\n";
  result+=description;
 }
 return result;
}
struct Api {
 using Size=DWORD(WINAPI*)(const wchar_t*,DWORD*);
 using Read=BOOL(WINAPI*)(const wchar_t*,DWORD,DWORD,void*);
 using Query=BOOL(WINAPI*)(const void*,const wchar_t*,void**,unsigned*);
 HMODULE module=nullptr;Size size=nullptr;Read read=nullptr;Query query=nullptr;DWORD error=0;
 Api(){
  module=LoadLibraryExW(L"version.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
  if(!module){error=GetLastError();if(!error)error=ERROR_MOD_NOT_FOUND;return;}
  size=reinterpret_cast<Size>(GetProcAddress(module,"GetFileVersionInfoSizeW"));
  read=reinterpret_cast<Read>(GetProcAddress(module,"GetFileVersionInfoW"));
  query=reinterpret_cast<Query>(GetProcAddress(module,"VerQueryValueW"));
  if(!size||!read||!query)error=ERROR_PROC_NOT_FOUND;
  // Keep this system module loaded for the process lifetime, like the shared
  // immutable function table; no additional link dependency for Host modules.
 }
};
inline std::wstring read(const std::wstring& path,DWORD& error){
 error=0;static const Api api;
 if(api.error){error=api.error;return {};}
 DWORD ignored=0;const DWORD bytes=api.size(path.c_str(),&ignored);
 if(!bytes){
  const DWORD code=GetLastError();
  if(code!=ERROR_RESOURCE_DATA_NOT_FOUND&&code!=ERROR_RESOURCE_TYPE_NOT_FOUND&&
     code!=ERROR_RESOURCE_NAME_NOT_FOUND&&code!=ERROR_RESOURCE_LANG_NOT_FOUND)error=code;
  return {};
 }
 if(bytes>16u*1024*1024){error=ERROR_BAD_LENGTH;return {};}
 std::vector<unsigned char> data(bytes);
 if(!api.read(path.c_str(),0,bytes,data.data())){error=GetLastError();return {};}
 return readDescriptions([&](const wchar_t* key,const void** out,unsigned* count){
  void* value=nullptr;
  if(!api.query(data.data(),key,&value,count))return false;
  *out=value;return true;
 });
}
} }
