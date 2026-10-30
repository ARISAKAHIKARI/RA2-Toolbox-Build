#pragma once
#include <cstdint>
#include <array>
#include <initializer_list>
#include "feature_type_config.h"

// Availability is separate from saved preference: switching a parent off must
// never erase a child's checked value, key, quota or position.
namespace assist { namespace settingsdeps {
enum class Need : unsigned {
 None, Render, Host64, GPU, Detail, Missile, LowDamage, ControlAll, Formation,
 Watch, WatchSecondary, Minimap, MinimapPrimary, MinimapSecondary, CustomMinimap,
 ShiftClick, AppendClicks, Ore, CustomMiners, CustomRefineries, Production,
 ManualRate, Capacity, Advanced, C4, Dodge, Construction, BuildAnywhere,
 Aircraft, AircraftQuotas, QuotaTarget, QuotaOverride, Repair, Quality, Reconstruction, TypeListFirst
};
constexpr Need typeListNeed(int feature){return Need(unsigned(Need::TypeListFirst)+unsigned(feature));}
static_assert(unsigned(Need::TypeListFirst)+featuretypes::Count<=64,"dependency state covers every type filter");
struct State {
 uint64_t available=0;
 void set(Need need,bool on){const auto bit=uint64_t(1)<<unsigned(need);if(on)available|=bit;else available&=~bit;}
 bool has(Need need)const{return need==Need::None||(available&(uint64_t(1)<<unsigned(need)))!=0;}
};
struct Decision {
 Need missing=Need::None;
 bool enabled()const{return missing==Need::None;}
};
inline Decision require(const State&state,std::initializer_list<Need> parents){for(auto parent:parents)if(!state.has(parent))return {parent};return {};}
inline const wchar_t* reason(Need need){
 if(unsigned(need)>=unsigned(Need::TypeListFirst)&&unsigned(need)<unsigned(Need::TypeListFirst)+featuretypes::Count)return L"需先开启此功能的类型清单限制";
 switch(need){
 case Need::Render:return L"需先开启渲染升级";
 case Need::Host64:return L"需要 64 位工具箱";
 case Need::GPU:return L"需先开启 Vulkan GPU";
 case Need::Detail:return L"需先选择精细场景";
 case Need::Quality:return L"需先开启额外画质处理";
 case Need::Reconstruction:return L"需先开启 xBRZ 边缘重建";
 case Need::Missile:return L"需先开启防空火控";
 case Need::LowDamage:return L"需先开启低伤害过滤";
 case Need::ControlAll:return L"需先开启跨玩家操控";
 case Need::Formation:return L"需先开启队形排列";
 case Need::Watch:return L"需先开启主屏或副屏跟踪";
 case Need::WatchSecondary:return L"需先开启副屏跟踪";
 case Need::Minimap:return L"需先开启主屏或副屏小地图";
 case Need::MinimapPrimary:return L"需先开启游戏内小地图";
 case Need::MinimapSecondary:return L"需先开启副屏小地图";
 case Need::CustomMinimap:return L"需先选择自定义位置";
 case Need::ShiftClick:return L"需先开启 Shift 五连点";
 case Need::AppendClicks:return L"当前模式不追加点击";
 case Need::Ore:return L"需先开启矿车自动卸矿";
 case Need::CustomMiners:return L"需先指定矿车类型";
 case Need::CustomRefineries:return L"需先指定矿场类型";
 case Need::Production:return L"需先开启生产队列提醒";
 case Need::ManualRate:return L"需先关闭自适应刷新率";
 case Need::Capacity:return L"需先开启载具载员信息";
 case Need::Advanced:return L"需先开启高级移动攻击";
 case Need::C4:return L"需先开启步兵强制进入";
 case Need::Dodge:return L"需先开启自动躲弹";
 case Need::Construction:return L"需先开启生产预定";
 case Need::BuildAnywhere:return L"需先开启随处建造";
 case Need::Aircraft:return L"需先开启飞机分配攻击";
 case Need::AircraftQuotas:return L"需先选择按数量配额";
 case Need::QuotaTarget:return L"需先输入有效目标类型";
 case Need::QuotaOverride:return L"需先开启此类型独立配额";
 case Need::Repair:return L"需先开启自动维修";
 default:return L"";
 }
}
inline Decision decision(int id,const State&s){
 if(id>=9201&&id<9200+2*featuretypes::Count&&(id&1)){
  const int feature=(id-9201)/2;
  if(feature!=featuretypes::BurstRefresh)return require(s,{typeListNeed(feature)});
 }
 switch(id){
 case 9306:case 9313:return require(s,{Need::Render});
 case 9304:case 9307:case 9308:case 9309:case 9314:return require(s,{Need::Render,Need::Quality});
 case 9305:return require(s,{Need::Render,Need::Quality,Need::Reconstruction});
 case 9311:return require(s,{Need::Host64,Need::Render,Need::GPU,Need::Detail});
 case 9040:case 9041:case 9181:case 881:return require(s,{Need::Missile});
 case 9183:return require(s,{Need::LowDamage});
 case 9193:return require(s,{Need::ControlAll});
 case 9056:return require(s,{Need::Formation});
 case 9061:return require(s,{Need::Watch});
 case 9164:case 9165:return require(s,{Need::WatchSecondary});
 case 9065:return require(s,{Need::Minimap});
 case 831:case 850:return require(s,{Need::MinimapPrimary});
 case 851:case 852:case 9100:case 9101:return require(s,{Need::MinimapPrimary,Need::CustomMinimap});
 case 9166:case 9167:case 9168:return require(s,{Need::MinimapSecondary});
 case 796:return require(s,{Need::ShiftClick});
 case 797:return require(s,{Need::ShiftClick,Need::AppendClicks});
 case 781:case 782:return require(s,{Need::Ore});
 case 783:return require(s,{Need::Ore,Need::CustomMiners});
 case 784:return require(s,{Need::Ore,Need::CustomRefineries});
 case 9070:case 9071:case 9072:case 9073:case 9074:case 9075:return require(s,{Need::Production});
 case 303:return require(s,{Need::Capacity});
 case 310:return require(s,{Need::ManualRate});
 case 787:case 818:case 802:return require(s,{Need::Advanced});
 case 812:case 9180:return require(s,{Need::C4});
 case 895:return require(s,{Need::Dodge});
 case 817:return require(s,{Need::Construction});
 case 9096:return require(s,{Need::BuildAnywhere});
 case 312:return require(s,{Need::Repair});
 case 710:case 711:case 712:case 713:case 714:case 751:return require(s,{Need::Aircraft,Need::AircraftQuotas});
 case 715:return require(s,{Need::Aircraft,Need::AircraftQuotas,Need::QuotaTarget});
 case 716:case 717:case 718:return require(s,{Need::Aircraft,Need::AircraftQuotas,Need::QuotaTarget,Need::QuotaOverride});
 default:return {};
 }
}
inline bool effective(bool saved,int id,const State&s){return saved&&decision(id,s).enabled();}
inline constexpr int BasicControls[]={
 9304,9305,9306,9307,9308,9309,9311,9313,9314,9040,9041,9181,881,9183,9193,9056,
 9061,9164,9165,9065,831,850,851,852,9100,9101,9166,9167,9168,796,797,
 781,782,783,784,9070,9071,9072,9073,9074,9075,303,310,787,818,802,812,
 9180,895,817,9096,710,711,712,713,714,751,715,716,717,718,312
};
inline constexpr auto Controls=[](){
 std::array<int,sizeof(BasicControls)/sizeof(BasicControls[0])+featuretypes::Count-1> ids{};
 unsigned next=0;for(int id:BasicControls)ids[next++]=id;
 for(int feature=0;feature<featuretypes::Count;++feature)if(feature!=featuretypes::BurstRefresh)ids[next++]=9201+2*feature;
 return ids;
}();
}}
