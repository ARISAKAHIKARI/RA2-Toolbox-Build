#pragma once
#include "ore_type_config.h"
namespace assist { namespace featuretypes {
// Filters narrow native eligibility. They never mutate rules or grant a new ability.
enum Feature {Aircraft,Disk,Magnetron,AdvancedAttack,LoadPassenger,LoadTransport,
 EnterSource,EnterTarget,UnloadTransport,UnloadPassenger,EscapeTransport,EscapePassenger,
 SellVehicle,RepairBuilding,AntiTheftDefense,MissileDefense,MissileTarget,BurstRefresh,Dodge,MagnetronDrop,Count};
struct Filter {uint32_t custom=0;int count=0;char ids[oreconfig::Limit][24]{};};
struct Config {Filter filters[Count]{};};
inline const wchar_t* key(int n){static const wchar_t* const names[]={L"Aircraft",L"Disk",L"Magnetron",L"AdvancedAttack",L"LoadPassenger",L"LoadTransport",L"EnterSource",L"EnterTarget",L"UnloadTransport",L"UnloadPassenger",L"EscapeTransport",L"EscapePassenger",L"SellVehicle",L"RepairBuilding",L"AntiTheftDefense",L"MissileDefense",L"MissileTarget",L"BurstRefresh",L"Dodge",L"MagnetronDrop"};return n>=0&&n<Count?names[n]:L"";}
inline const wchar_t* name(int n){static const wchar_t* const names[]={L"飞机分配攻击",L"飞碟主武器",L"磁电轮换攻击",L"高级移动攻击",L"自动装车 · 乘员",L"自动装车 · 载具",L"高级进驻 · 乘员",L"高级进驻 · 建筑或载具",L"智能卸载 · 载具",L"智能卸载 · 乘员后续任务",L"逃生助手 · 载具",L"逃生助手 · 乘员",L"变卖战车",L"自动维修与维修快捷键",L"防偷点杀 · 防御建筑",L"火控拦截 · 防空单位",L"火控拦截 · 空中目标类型",L"强制刷弹 · 自动启用类型",L"自动躲弹 · 参与单位",L"磁电自动摔落"};return n>=0&&n<Count?names[n]:L"";}
inline const wchar_t* initial(int n){return n==AntiTheftDefense?L"NALASR,GAPILL,YAGGUN":n==Aircraft?L"ORCA,BEAG":n==Disk?L"DISK":(n==Magnetron||n==MagnetronDrop)?L"TELE":L"";}
inline bool valid(const Config&c){for(const auto&f:c.filters){if(f.custom>1||f.count<0||f.count>oreconfig::Limit)return false;
 for(int i=0;i<f.count;++i){if(!f.ids[i][0]||!memchr(f.ids[i],0,24))return false;
  for(int j=0;f.ids[i][j];++j){char x=oreconfig::upper(f.ids[i][j]);if(!((x>='A'&&x<='Z')||(x>='0'&&x<='9')||x=='_'||x=='-'))return false;}}}return true;}
inline bool allowed(const Config&c,int feature,const char*id){if(feature<0||feature>=Count)return false;const auto&f=c.filters[feature];if(!f.custom)return true;
 if(!id||f.count<0||f.count>oreconfig::Limit)return false;for(int i=0;i<f.count;++i)if(oreconfig::equal(f.ids[i],id))return true;return false;}
}}
