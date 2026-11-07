#pragma once
#include <cwctype>
#include <string>

namespace external { namespace discovery {
// Discovery aliases only. Runtime still validates layouts and each entry it uses.
inline bool isGameName(const wchar_t* text){
 if(!text)return false;
 std::wstring name(text);
 for(auto& c:name)c=static_cast<wchar_t>(std::towlower(c));
 return name==L"gamemd.exe"||name==L"gamemd-spawn.exe"||
        name==L"gamemd-ares.exe"||name==L"gamemdares.exe"||name==L"gameares.exe"||
        name==L"gamemd.dat"||name==L"gamemd-spawn.dat"||
        name==L"gamemd-ares.dat"||name==L"gamemdares.dat"||name==L"gameares.dat";
}
} }
