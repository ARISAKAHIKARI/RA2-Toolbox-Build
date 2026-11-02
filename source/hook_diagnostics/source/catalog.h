#pragma once
#include <vector>
#include <algorithm>
#include "../../assist/source/hook_registry.h"
#include "../../shared/source/experimental_patch_policy.h"
namespace hooktest {
enum Kind { ReadOnly, RuntimeOnly, FrameSignal, Tooltip, Mega, Placement, Camera, LowDamage, AirType, AirFull, Control,
 ShroudClear, ShroudHalf, TechPair, TechMaster, DirectPlacement, Network, CommandOwner, Sell, Crc, DirectMap };
struct Step {Kind kind;const wchar_t*name;std::vector<uint32_t>sites;};
inline const std::vector<uint32_t>&networkSites(){static const std::vector<uint32_t>v={0x649CA0,0x649D61,0x649DBC,0x649E1E,0x64B4EA,0x64B660,0x64B9D4};return v;}
inline std::vector<uint32_t>withNetwork(std::initializer_list<uint32_t>extra){auto v=networkSites();v.insert(v.end(),extra);return v;}
inline const std::vector<Step>&steps(){static const std::vector<Step>v={
 {ReadOnly,L"只读基线（不加载 Runtime）",{}},
 {RuntimeOnly,L"Windows 消息 Hook + Runtime，功能全关",{}},
 {FrameSignal,L"高危：帧完成通知",{0x55DE81}},
 {Tooltip,L"高危：建造时间提示",{0x7F74D8}},
 {Mega,L"高危：原生任务成功通知",{0x646F4E}},
 {Placement,L"高危：建造位置判定",{0x4A8EB0}},
 {Camera,L"高危：小地图尺寸相机边界（满足原几何条件才安装）",{0x6D8640}},
 {LowDamage,L"高危：原生低伤害过滤（联机实验）",{0x6F7D44}},
 {AirType,L"高危：飞机路径点下单",{0x7E2908}},
 {AirFull,L"高危：飞机完整原生路径（成组）",{0x416AB6,0x7E2908}},
 {Control,L"高危：控制权判定（3 处成组）",{0x6339BA,0x637E40,0x50B6F0}},
 {ShroudClear,L"高危：透明黑幕绘制（2 处成组）",{0x47F038,0x47F274}},
 {ShroudHalf,L"高危：半透明黑幕绘制（相同 2 处）",{0x47F038,0x47F274}},
 {TechPair,L"高危：侧栏资格判定（2 处成组）",{0x6A97D2,0x6AA781}},
 {TechMaster,L"高危：侧栏兼容判定（5 处成组）",{0x44574A,0x445795,0x4457D0,0x445810,0x6AA788}},
 {DirectPlacement,L"高危：直接建造判定（2 处成组）",{0x4A8EC3,0x4A918E}},
 {Network,L"高危：网络所属方保留（7 处成组）",networkSites()},
 {CommandOwner,L"高危：命令所属方（4 处 + 控制/网络依赖）",withNetwork({0x6339BA,0x637E40,0x50B6F0,0x646EF5,0x646FD2,0x6FFC93,0x6FFE2B})},
 {Sell,L"高危：原生出售（2 处 + 网络依赖）",withNetwork({0x6929E5,0x4AC179})},
 {Crc,L"高危：CRC MapIsClear 适配（8 处；不执行 Reveal）",{0x64DCCA,0x4F576B,0x4FC328,0x577ACA,0x577BC7,0x577CA7,0x577DB0,0x577F48}},
 {DirectMap,L"高危：直接地图显示（5 处成组，最后测试）",{0x508E60,0x508EBA,0x508F8F,0x577B3A,0x6D88FC}}
 };return v;}
inline bool dormant(uint32_t a){return a==0x51A4D8||a==0x51C32B||a==0x4A9840;}
// This slot is owned by the Cursor service, not the Assist-only test session.
inline bool cursorOnly(uint32_t a){return a==experimentalpatch::CursorTooltipLayout;}
// Explicit single-player experiment, excluded from generic OOS scheduling.
inline bool experimentalOnly(uint32_t a){return a==experimentalpatch::PauseLoop||nativesidebarpolicy::find(a);}
inline std::vector<uint32_t>allSites(){std::vector<uint32_t>out;for(uint32_t a=0x400000;a<0x800000;++a)if(experimentalpatch::size(a))out.push_back(a);return out;}
inline bool covered(uint32_t a){for(const auto&s:steps())if(std::find(s.sites.begin(),s.sites.end(),a)!=s.sites.end())return true;return false;}
}
