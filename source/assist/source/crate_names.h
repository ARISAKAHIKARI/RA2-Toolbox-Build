#pragma once
namespace assist {
inline const wchar_t* crateName(int reward){
 static const wchar_t* const names[]={L"金钱",L"单位",L"全军治疗",L"隐形",L"爆炸",L"燃烧爆炸",L"步兵小队",L"黑幕",L"地图揭示",L"装甲强化",L"速度强化",L"火力强化",L"核弹",L"无敌",L"晋升",L"离子风暴",L"毒气",L"矿石",L"空降舱"};
 return reward>=0&&reward<19?names[reward]:L"随机";
}
}
