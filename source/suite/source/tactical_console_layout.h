#pragma once
#include <initializer_list>
#include "settings_categories.h"
#include "../../assist/source/feature_type_config.h"
#include "../../assist/source/ui_layout.h"

namespace suite::consolelayout {
struct Feature {
 int key;
 int page;
 const wchar_t* section;
 const wchar_t* title;
 bool primary;
 std::initializer_list<int> controls;
};

// The original HWND IDs remain the settings owners. The key identifies a
// card, and only a primary control stays visible when its settings are folded.
inline constexpr Feature Features[]={
 // Each gesture owns its shortcut / mouse choice under the same settings fold.
 {477,categories::Control,L"移动、攻击与操作手势",L"磁电轮换攻击",true,{477,478,730}},
 {702,categories::Control,L"移动、攻击与操作手势",L"高级进驻",true,{702,703,704,732}},
 {719,categories::Control,L"移动、攻击与操作手势",L"飞碟主武器攻击",true,{719,725,726,735}},
 {720,categories::Control,L"移动、攻击与操作手势",L"出售被吸取目标",true,{720,727,728,737}},
 {721,categories::Control,L"移动、攻击与操作手势",L"出售战车",true,{721,789,738}},
 {795,categories::Control,L"移动、攻击与操作手势",L"Shift 五连点",true,{795,796,797}},
 // The legacy "下令与巡逻参数" button is only a Win32 presentation fold.
 // Each command owns its own switch, key, mouse and related parameters.
 {9400,categories::Control,L"队形、巡逻与飞机路径",L"模糊下令",true,{9400,9440,9441,9480,9481}},
 {9401,categories::Control,L"队形、巡逻与飞机路径",L"区域目标分配",true,{9401,9442,9443}},
 {9402,categories::Control,L"队形、巡逻与飞机路径",L"自定义巡逻圈",true,{9402,9444,9445,9482,9483,9484,9485,9486}},
 {803,categories::Control,L"队形、巡逻与飞机路径",L"飞机路径点",true,{803,894,866}},
 {700,categories::Control,L"进驻、装卸与超武目标选择",L"装车策略与范围",true,{700,701}},
 {885,categories::Automatic,L"自动战斗与防空",L"自动躲弹",true,{885,895}},
 {9530,categories::Automatic,L"维修与自动救援",L"绑弹步兵自动上车",true,{9530,9531,9532}},
 {9494,categories::Rules,L"单机速度与暂停",L"单机游戏速度",true,{9494,9495}},
 {9403,categories::Rules,L"单机速度与暂停",L"暂停与逐帧",true,{9403,9454,9455,9406}},
 {420,categories::Rules,L"科技与建造",L"科技全开",true,{420,896,9753,421,422,423,424,425,426,427,428,814,815}},
 {9097,categories::Rules,L"科技与建造",L"科技加强",true,{9097,9470,9472,9471,9098}},
 {476,categories::Rules,L"科技与建造",L"建筑放置",true,{476,9096,9251,9252,807}},
 {808,categories::Rules,L"地图显示与揭示",L"黑幕绘制",true,{808,809}},
 {9492,categories::Rules,L"地图显示与揭示",L"硬件黑幕显示",true,{9492,9493}},
 {9199,categories::Rules,L"地图显示与揭示",L"手动地图揭示",true,{9199,108,208}},
 {860,categories::Rules,L"操控与变卖",L"跨玩家操控",true,{860,9250,9193,865,9194}},
 {9189,categories::Rules,L"操控与变卖",L"原生变卖光标",true,{9189,9190}},
 {9473,categories::Rules,L"运行状态",L"硬件断点预算",false,{9473}},

 {assist::uilayout::NativePipToggle,categories::Experimental,L"视角与界面",L"原生画中画",true,{assist::uilayout::NativePipToggle,assist::uilayout::NativePipShortcut,assist::uilayout::NativePipPin,assist::uilayout::NativePipStatus}},
 {9090,categories::Experimental,L"视角与界面",L"视角缩放",true,{9090,9091,9092,9093,9450,9451,9452}},
 {9463,categories::Experimental,L"视角与界面",L"小图相机边界",true,{9463,9464}},
 {9462,categories::Experimental,L"视角与界面",L"侧栏显示",true,{9462,9404,9453,9407}},
 {9536,categories::Experimental,L"剧情触发器",L"保持操作权限",true,{9536,9537}},
 {9538,categories::Experimental,L"剧情触发器",L"触发视角保护",true,{9538,9539}},
 {9750,categories::Experimental,L"命令与兼容",L"操控入队",true,{9750,9751}},
 {761,categories::Experimental,L"命令与兼容",L"指令兼容模式",false,{761,762}},
 {9094,categories::Experimental,L"命令与兼容",L"Sticky 驻守",true,{9094,9095}},
 {9191,categories::Experimental,L"命令与兼容",L"原生命令仲裁",true,{9191,9192}},
 {9198,categories::Experimental,L"命令与兼容",L"精确原生战斗查询",true,{9198}},
 {9187,categories::Experimental,L"命令与兼容",L"事件所属方保留",true,{9187,9188}},
 {9196,categories::Experimental,L"规则实验",L"地图 CRC 原值镜像",true,{9196,9197}},
 {9490,categories::Experimental,L"规则实验",L"持续授予超武",true,{9490,9491}},
 {9253,categories::Experimental,L"规则实验",L"所选单位所属方自毁",true,{9253,9254,9185,9186}},
 {810,categories::Experimental,L"暂不可用",L"步兵强制进入建筑",false,{810,811,812,9180,813}},
 {861,categories::Experimental,L"暂不可用",L"切换操控方",false,{861,862,863,864}},
 {722,categories::Experimental,L"暂不可用",L"快捷键变卖对手建筑",false,{722,9540}},
 {9496,categories::Automatic,L"采矿、倒矿与防蜘蛛",L"自动点杀蜘蛛",true,{9496,9505,9240,9241,9497,9498}},
};

inline const Feature* feature(int id){
 for(const auto& entry:Features)for(int control:entry.controls)if(control==id)return &entry;
 return nullptr;
}

struct TypeScope {
 int owner;
 const wchar_t* title;
 const wchar_t* toggle;
 const wchar_t* editor;
};

inline const TypeScope* typeScope(int index){
 using namespace assist::featuretypes;
 static constexpr TypeScope scopes[]={
  {Aircraft,L"飞机分配攻击",L"限定飞机类型",L"飞机类型 ID"},
  {Disk,L"飞碟主武器",L"限定飞碟类型",L"飞碟类型 ID"},
  {Magnetron,L"磁电轮换攻击",L"限定磁电类型",L"磁电类型 ID"},
  {AdvancedAttack,L"高级移动攻击",L"限定参战单位",L"参战单位类型 ID"},
  {LoadPassenger,L"自动装车",L"限定乘员类型",L"乘员类型 ID"},
  {LoadPassenger,L"自动装车",L"限定载具类型",L"载具类型 ID"},
  {EnterSource,L"高级进驻",L"限定参与单位",L"参与单位类型 ID"},
  {EnterSource,L"高级进驻",L"限定目标建筑或载具",L"目标建筑／载具类型 ID"},
  {UnloadTransport,L"智能卸载",L"限定卸载载具",L"卸载载具类型 ID"},
  {UnloadTransport,L"智能卸载",L"限定执行后续任务的乘员",L"后续任务乘员类型 ID"},
  {EscapeTransport,L"逃生助手",L"限定逃生载具",L"逃生载具类型 ID"},
  {EscapeTransport,L"逃生助手",L"限定逃生乘员",L"逃生乘员类型 ID"},
  {SellVehicle,L"变卖战车",L"限定可变卖载具",L"可变卖载具类型 ID"},
  {RepairBuilding,L"维修",L"限定参与维修的建筑",L"维修建筑类型 ID"},
  {AntiTheftDefense,L"防偷点杀",L"限定参与防御的建筑",L"防御建筑类型 ID"},
  {MissileDefense,L"防空火控",L"限定参与拦截的防空单位",L"防空单位类型 ID"},
  {MissileDefense,L"防空火控",L"限定空中目标",L"空中目标类型 ID"},
  {BurstRefresh,nullptr,nullptr,nullptr}, // Obsolete INI entry, no adjustable control.
  {Dodge,L"自动躲弹",L"限定参与躲避的单位",L"躲避单位类型 ID"},
  {MagnetronDrop,L"磁电自动摔落",L"限定参与摔落的磁电",L"摔落磁电类型 ID"},
  {SpiderDefense,L"自动点杀蜘蛛",L"限定参与点杀的单位",L"点杀单位类型 ID"},
 };
 static_assert(sizeof(scopes)/sizeof(scopes[0])==Count,"every saved filter has a presentation decision");
 return index>=0&&index<Count&&scopes[index].title?&scopes[index]:nullptr;
}

inline const wchar_t* label(int id){
 if(id>=9200&&id<9200+2*assist::featuretypes::Count){
  const auto* scope=typeScope((id-9200)/2);
  return scope?(id%2?scope->editor:scope->toggle):nullptr;
 }
 switch(id){
 case 9494:return L"目标游戏帧率（0 = 原版）";
 case 9403:return L"高危：暂停与逐帧（仅单机）";
 case 9454:return L"暂停／恢复快捷键";
 case 9455:return L"前进一帧快捷键";
 case 896:return L"科技全开实现";
 case 421:return L"搜索科技许可";
 case 422:return L"科技许可清单";
 case 9470:return L"科技加强实现";
 case 9096:return L"建筑放置方式";
 case 9251:return L"已完成建筑";
 case 9252:return L"放置预览快捷键";
 case 808:return L"黑幕绘制方式";
 case 208:return L"手动揭示快捷键";
 case 9250:return L"操控权限实现";
 case assist::uilayout::NativePipShortcut:return L"固定区域／跟随单位快捷键";
 case 9091:return L"视角倍率";
 case 9450:return L"缩小视野快捷键";
 case 9451:return L"放大视野快捷键";
 case 9452:return L"恢复 100% 快捷键";
 case 9404:return L"右侧建造栏";
 case 9453:return L"切换侧栏快捷键";
 case 9750:return L"操控入队实现";
 case 9095:return L"驻守快捷键";
 case 761:return L"移动指令兼容";
 case 762:return L"维修与出售兼容";
 case 811:return L"强制进入快捷键";
 case 812:return L"强制进入鼠标键";
 case 9180:return L"强制进入方式";
 case 862:return L"目标所属方";
 case 9254:return L"自毁快捷键";
 case 783:return L"矿车类型 ID";
 case 784:return L"矿场类型 ID";
 default:return nullptr;
 }
}
}
