#pragma once
namespace assist { namespace buildingmodes {
constexpr int PlacementEvent=2,TechData=3,TechParameters=4;
constexpr int placement(int mode){return mode>=0&&mode<=PlacementEvent?mode:0;}
constexpr int technology(int mode){return mode>=0&&mode<=TechParameters?mode:0;}
constexpr bool toolOrders(int requested,int applied){return requested!=TechData&&requested!=TechParameters&&applied!=TechData+1&&applied!=TechParameters+1;}
inline const wchar_t* placementCaption(int mode){return mode==PlacementEvent?L"建筑放置（原生事件）":L"高危：随处建造";}
inline const wchar_t* technologyCaption(int mode){return mode==TechParameters?L"实验性：原生面板科技（参数拦截，不改指令／规则）":mode==TechData?L"高危：原生面板科技（规则数据）":L"高危：科技解锁";}
}}
