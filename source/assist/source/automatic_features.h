#pragma once
#include <cstdint>
namespace assist { namespace autofeatures {
// Only persistent automatic behavior switches belong here. Type filters,
// numeric parameters and explicit commands (including AutoBoard) do not.
enum Feature {Repair,Eject,MagnetronDrop,ConstructionQueue,OreReturn,MinerDefense,AntiTheft,Dodge,MissileDefense,MissileReserve,LowDamageFilter,MagnetronGlobal,AutoCS,BombBoard,Count};
struct Entry {Feature feature;int control,keyControl,action;const wchar_t*name;const wchar_t*setting;};
inline constexpr Entry Entries[]={
 {Repair,100,200,1000,L"自动维修",nullptr},
 {Eject,455,456,1696,L"载员自动逃生",nullptr},
 {MagnetronDrop,899,9502,1802,L"磁电全局自动摔落",L"MagnetronDrop"},
 {ConstructionQueue,799,9503,1803,L"生产预定",L"ConstructionQueue"},
 {OreReturn,470,9504,1804,L"矿车自动卸矿",L"OreReturn"},
 {MinerDefense,9496,9505,1805,L"自动点杀蜘蛛",L"MinerDefense"},
 {AntiTheft,804,9506,1806,L"防御建筑优先攻击渗透单位",L"AntiTheft"},
 {Dodge,885,9507,1807,L"自动躲弹",L"Dodge"},
 {MissileDefense,880,9508,1808,L"防空火控",L"MissileDefense"},
 {MissileReserve,9181,9509,1809,L"提前休眠留枪",L"MissileReserve"},
 {LowDamageFilter,9182,9510,1810,L"过滤低伤害自动目标",L"LowDamageFilter"},
 {MagnetronGlobal,9042,9511,1811,L"磁电全局无限吸",L"MagnetronGlobal"},
 {AutoCS,446,447,1699,L"自动移动攻击（CS）",nullptr},
 {BombBoard,9530,9535,1813,L"绑弹步兵自动上车",L"BombBoard"}
};
static_assert(sizeof(Entries)/sizeof(Entries[0])==Count,"one entry per automatic feature");
inline const Entry*byControl(int id){for(const auto&entry:Entries)if(entry.control==id)return &entry;return nullptr;}
inline const Entry*byKeyControl(int id){for(const auto&entry:Entries)if(entry.keyControl==id)return &entry;return nullptr;}
inline const Entry*byAction(int id){for(const auto&entry:Entries)if(entry.action==id)return &entry;return nullptr;}
// Legacy bindings remain in their original Hotkeys/CS/Eject settings. Newly
// introduced bindings are optional and must never overwrite those values.
template<class Read>inline void loadKeys(uint32_t(&keys)[Count],Read read){for(const auto&entry:Entries)if(entry.setting)keys[entry.feature]=uint32_t(read(L"AutomaticHotkeys",entry.setting,0))&65535u;}
template<class Write>inline void saveKeys(const uint32_t(&keys)[Count],Write write){for(const auto&entry:Entries)if(entry.setting)write(L"AutomaticHotkeys",entry.setting,keys[entry.feature]&65535u);}
}}
