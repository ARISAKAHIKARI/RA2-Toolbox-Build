#pragma once
#include <stdint.h>
#include <string>
#include <cstdio>
namespace gamebuild {
// Read-only startup evidence. These hints never grant patch/command permission.
template<class Read> std::wstring describe(Read read){
 uint16_t mz=0,machine=0;uint32_t pe=0,signature=0,stamp=0,size=0;
 if(!read(0x400000,&mz,2)||mz!=0x5A4D||!read(0x40003C,&pe,4)||pe<64||pe>0x100000||
    !read(0x400000+pe,&signature,4)||signature!=0x4550||!read(0x400004+pe,&machine,2)||
    !read(0x400008+pe,&stamp,4)||!read(0x400050+pe,&size,4))return L"game-build header=unavailable";
 const wchar_t* hint=stamp==4261544450u&&size==0x7CC000?L"user-sample-3-ZhanWang":
  stamp==33686018u&&size==0x7CB000?L"user-sample-4-LBW":L"unknown";
 std::wstring out=L"game-build hint=";out+=hint;out+=L" (PE metadata; not full file identity)";
 wchar_t number[80];swprintf(number,80,L" machine=%04X stamp=%08X imageSize=%08X",unsigned(machine),unsigned(stamp),unsigned(size));out+=number;
 const uint32_t addresses[]={0x55DE73,0x4C794B,0x416AB6,0x7E2908,0x4C65E0,0x4C6B60};
 for(auto at:addresses){uint8_t bytes[10]{};swprintf(number,80,L" probe-%08X=",unsigned(at));out+=number;
  if(!read(at,bytes,sizeof bytes)){out+=L"unreadable";continue;}
  for(auto b:bytes){swprintf(number,80,L"%02X",unsigned(b));out+=number;}
 }
 return out;
}
}
