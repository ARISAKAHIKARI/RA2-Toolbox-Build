#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cwctype>
#include <string>

// File-only recognition for the user-supplied renamed YR build. This is a
// discovery fingerprint, not permission to patch: Runtime validates every site.
namespace external { namespace imageidentity {
constexpr uint64_t FileBytes=4813161;
inline bool randomName(const wchar_t* text){
 if(!text)return false;std::wstring s(text);
 for(auto&c:s)c=static_cast<wchar_t>(towlower(c));
 if(s.size()>4&&(s.substr(s.size()-4)==L".exe"||s.substr(s.size()-4)==L".dat"))s.resize(s.size()-4);
 if(s.size()<8||s.size()>32)return false;
 for(auto c:s)if(!((c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f')))return false;
 return true; // A candidate hint only; never accepts a process by itself.
}
inline uint32_t word(const unsigned char*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
template<class Read> bool matches(Read read,uint64_t bytes){
 if(bytes!=FileBytes)return false;
 unsigned char dos[64]{},nt[84]{};
 if(!read(0,dos,sizeof dos)||dos[0]!='M'||dos[1]!='Z')return false;
 uint32_t at=word(dos+60);if(at<64||at>4096||!read(at,nt,sizeof nt))return false;
 if(word(nt)!=0x4550||nt[4]!=0x4c||nt[5]!=1||nt[24]!=0x0b||nt[25]!=1||
    word(nt+40)!=0x3cd80f||word(nt+52)!=0x400000||word(nt+80)!=0x794000)return false;
 struct Mark{uint32_t offset,size;unsigned char bytes[28];};
 static constexpr Mark marks[]={
  {0x2339b0,16,{0x8b,0x89,0x1c,0x02,0x00,0x00,0x56,0x8b,0x71,0x30,0xe8,0x31,0x7d,0xed,0xff,0x84}},
  {0x237e37,20,{0x8b,0x8f,0x1c,0x02,0x00,0x00,0x8b,0x71,0x30,0xe8,0xab,0x38,0xed,0xff,0x84,0xc0,0x74,0x0c,0xa1,0x4c}},
  {0x3b8a00,16,{0x83,0xec,0x20,0x53,0x8b,0x5c,0x24,0x2c,0x56,0x8b,0xf1,0x85,0xdb,0x0f,0x84,0x2c}},
  {0x42d028,28,{0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0}}
 };
 for(const auto&m:marks){unsigned char got[28]{};if(!read(m.offset,got,m.size)||std::memcmp(got,m.bytes,m.size))return false;}
 return true;
}
} }
