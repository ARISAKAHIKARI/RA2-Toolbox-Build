#pragma once
#include <cstdint>
namespace assist { namespace compute {
enum Feature {Advanced,Magnetron,Ore,Defense,Enter,Aircraft,Repair,Load,Unload,Escape,MissileDefense,BurstRefresh,Dodge,MagnetronGlobal,MagnetronDrop,Count};
constexpr uint32_t All=(1u<<Count)-1,Default=(1u<<Repair)|(1u<<Load)|(1u<<Unload)|(1u<<Escape)|(1u<<MissileDefense)|(1u<<BurstRefresh)|(1u<<Dodge);
inline bool inside(uint32_t mask,Feature f){return (mask&(1u<<f))!=0;}
inline const wchar_t*key(int f){const wchar_t*k[]={L"AdvancedAttack",L"Magnetron",L"OreReturn",L"AntiTheft",L"AdvancedEnter",L"AircraftSpread",L"Repair",L"AutoLoad",L"SmartUnload",L"Ejection",L"MissileDefense",L"BurstRefresh",L"Dodge",L"MagnetronGlobal",L"MagnetronDrop"};return f>=0&&f<Count?k[f]:L"";}
inline const wchar_t*label(int f){const wchar_t*k[]={L"高级移动攻击",L"磁电轮换攻击",L"矿车自动卸矿",L"防御优先攻击",L"高级进驻",L"飞机分配攻击",L"自动维修",L"自动装车",L"智能卸载",L"载员自动逃生",L"防空导弹拦截",L"强制刷弹",L"自动躲弹",L"磁电全局无限吸",L"磁电全局自动摔落"};return f>=0&&f<Count?k[f]:L"";}
inline uint32_t magnetronIntents(uint32_t mask,bool runtime){return (inside(mask,Magnetron)==runtime?1u:0u)|(inside(mask,MagnetronGlobal)==runtime?2u:0u);}
inline uint32_t plannerFlags(uint32_t flags,uint32_t mask,bool runtime){
 const uint32_t bits[]={8192u,262144u,32768u,1073741824u};uint32_t out=0;
 for(int i=0;i<4;++i)if(i==1?magnetronIntents(mask,runtime)!=0:inside(mask,Feature(i))==runtime)out|=flags&bits[i];return out;
}
} }
