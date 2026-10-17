#pragma once
#include <cstdint>

// Read-only YR 1.001 sentinel checks. Never infer 'waiting for a match' from
// a failed memory read: wrong executable/layout and missing permissions differ.
namespace external { namespace layout {
enum class Stage : uint8_t {
 Ready, WaitingForMatch, UnreadableGameFlag, UnreadableLocalHouse,
 InvalidLocalHouse, UnreadableFrame, InvalidFrame, MissingLocalHouseInList, UnrecognizedUnitLayout
};
struct Result { Stage stage=Stage::WaitingForMatch; uint32_t address=0, value=0; };
inline const wchar_t* description(Stage stage) {
 switch(stage){
 case Stage::Ready:return L"读取正常";
 case Stage::WaitingForMatch:return L"游戏尚未进入对局";
 case Stage::UnreadableGameFlag:return L"无法读取对局状态（可能是地址布局或访问问题）";
 case Stage::UnreadableLocalHouse:return L"无法读取本地玩家指针（检查 EXE 版本）";
 case Stage::InvalidLocalHouse:return L"本地玩家指针无效（可能正在载入，或版本不匹配）";
 case Stage::UnreadableFrame:return L"无法读取游戏帧号（检查 EXE 版本）";
 case Stage::InvalidFrame:return L"游戏帧号无效（可能是地址布局不匹配）";
 case Stage::MissingLocalHouseInList:return L"玩家指针不在玩家数组中（可能是数据布局不匹配）";
 case Stage::UnrecognizedUnitLayout:return L"单位数组非空但类型标识不匹配（检查游戏版本）";
 }
 return L"未知读取状态";
}
template<class Read> Result inspect(Read read){
 uint8_t started=0;
 if(!read(0xA8E9A0,&started,sizeof started))return {Stage::UnreadableGameFlag,0xA8E9A0,0};
 if(!started)return {Stage::WaitingForMatch,0xA8E9A0,started};
 uint32_t house=0;
 if(!read(0xA83D4C,&house,sizeof house))return {Stage::UnreadableLocalHouse,0xA83D4C,0};
 if(house<0x10000||house>=0x80000000u)return {Stage::InvalidLocalHouse,0xA83D4C,house};
 int32_t frame=0;
 if(!read(0xA8ED84,&frame,sizeof frame))return {Stage::UnreadableFrame,0xA8ED84,0};
 if(frame<0)return {Stage::InvalidFrame,0xA8ED84,static_cast<uint32_t>(frame)};
 return {Stage::Ready,0,0};
}
} }
