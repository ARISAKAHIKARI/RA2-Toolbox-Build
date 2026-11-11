#pragma once
#include "display_settings.h"
#include <string>
#include <cwchar>
#include <cstdlib>
namespace suite {
enum class VideoGroup {Render,Monitor};
enum class VideoSection {Render,Camera,Map,Production,Information};
inline const wchar_t* videoSectionName(VideoSection section){
 switch(section){
  case VideoSection::Camera:return L"副相机";
  case VideoSection::Map:return L"小地图";
  case VideoSection::Production:return L"生产提醒";
  case VideoSection::Information:return L"战场提示";
  default:return L"渲染与效果";
 }
}
// Header controls are independent feature switches that remain available while
// their parameters are folded. The names point to static string literals.
struct VideoEntry {DisplayOption option;VideoGroup group;const wchar_t*label;const wchar_t*choices;VideoSection section;bool groupHeader=false;};
inline constexpr VideoEntry videoEntries[]={
 {OverlayToggleKey,VideoGroup::Render,L"所有覆盖层显隐键（默认 · / `）",L"",VideoSection::Render},
 {RenderUpgrade,VideoGroup::Render,L"渲染升级（入口暂不可用）",L"",VideoSection::Render,true},
 {SinglePlayerFPS,VideoGroup::Render,L"单机游戏 FPS（0 = 原版，10–240）",L"",VideoSection::Render},
 {RenderHighRefresh,VideoGroup::Render,L"高刷移动补帧（只读帧号取样）",L"",VideoSection::Render},
 {RenderPhysical,VideoGroup::Render,L"按显示器物理像素绘制",L"",VideoSection::Render},
 {RenderUnified,VideoGroup::Render,L"合并世界信息与监控覆盖层",L"",VideoSection::Render,true},
 {RenderGPU,VideoGroup::Render,L"Vulkan GPU（主视图、副相机、小地图）",L"",VideoSection::Render,true},
 {RenderParticles,VideoGroup::Render,L"粒子与动画效果",L"",VideoSection::Render,true},
 {RenderLasers,VideoGroup::Render,L"激光与光束效果",L"",VideoSection::Render,true},
 {RenderVoxelLighting,VideoGroup::Render,L"VXL 实时光影重算",L"",VideoSection::Render,true},
 {RenderSpriteLighting,VideoGroup::Render,L"SHP 实时光影重算",L"",VideoSection::Render,true},
 {WatchPrimary,VideoGroup::Monitor,L"在游戏内显示副相机",L"",VideoSection::Camera,true},
 {WatchSecondary,VideoGroup::Monitor,L"在第二显示器显示副相机",L"",VideoSection::Camera,true},
 {WatchDanger,VideoGroup::Monitor,L"自动跟踪危险载员",L"",VideoSection::Camera},
 {WatchKey1,VideoGroup::Monitor,L"副相机 1 快捷键",L"",VideoSection::Camera},
 {WatchKey2,VideoGroup::Monitor,L"副相机 2 快捷键",L"",VideoSection::Camera},
 {WatchKey3,VideoGroup::Monitor,L"副相机 3 快捷键",L"",VideoSection::Camera},
 {WatchX,VideoGroup::Monitor,L"副屏相机组水平位置 / 万分比",L"",VideoSection::Camera},
 {WatchY,VideoGroup::Monitor,L"副屏相机组垂直位置 / 万分比",L"",VideoSection::Camera},
 {MapPrimary,VideoGroup::Monitor,L"在游戏内显示小地图",L"",VideoSection::Map,true},
 {MapSecondary,VideoGroup::Monitor,L"在第二显示器显示小地图",L"",VideoSection::Map,true},
 {MapMode,VideoGroup::Monitor,L"小地图内容",L"示意|实景",VideoSection::Map},
 {MapScale,VideoGroup::Monitor,L"游戏内小地图缩放 / %",L"",VideoSection::Map},
 {MapPosition,VideoGroup::Monitor,L"游戏内小地图位置",L"原雷达左侧|左上|左下|自定义",VideoSection::Map},
 {MapX,VideoGroup::Monitor,L"自定义 X / 像素",L"",VideoSection::Map},
 {MapY,VideoGroup::Monitor,L"自定义 Y / 像素",L"",VideoSection::Map},
 {MapAlignX,VideoGroup::Monitor,L"水平对齐",L"靠左|居中|靠右",VideoSection::Map},
 {MapAlignY,VideoGroup::Monitor,L"垂直对齐",L"靠上|居中|靠下",VideoSection::Map},
 {MapSecondaryX,VideoGroup::Monitor,L"副屏小地图水平位置 / 万分比",L"",VideoSection::Map},
 {MapSecondaryY,VideoGroup::Monitor,L"副屏小地图垂直位置 / 万分比",L"",VideoSection::Map},
 {MapSecondaryWidth,VideoGroup::Monitor,L"副屏小地图宽度 / 像素",L"",VideoSection::Map},
 {ProductionHints,VideoGroup::Monitor,L"生产队列空闲提醒",L"",VideoSection::Production,true},
 {ReminderBuilding,VideoGroup::Monitor,L"提醒建筑队列",L"",VideoSection::Production},
 {ReminderDefense,VideoGroup::Monitor,L"提醒防御队列",L"",VideoSection::Production},
 {ReminderInfantry,VideoGroup::Monitor,L"提醒步兵队列",L"",VideoSection::Production},
 {ReminderVehicle,VideoGroup::Monitor,L"提醒载具队列",L"",VideoSection::Production},
 {ReminderShip,VideoGroup::Monitor,L"提醒舰船队列",L"",VideoSection::Production},
 {ReminderAircraft,VideoGroup::Monitor,L"提醒飞机队列",L"",VideoSection::Production},
 {RevealCloakedUnits,VideoGroup::Monitor,L"补绘未探测的隐形 / 潜水单位",L"",VideoSection::Information,true},
 {RevealMirageUnits,VideoGroup::Monitor,L"幻影显示真实坦克外形",L"",VideoSection::Information,true},
 {RevealEnemySpies,VideoGroup::Monitor,L"敌方间谍显示真实兵种和颜色",L"",VideoSection::Information,true},
 {HarvesterCounter,VideoGroup::Monitor,L"矿车指示器（工作中 / 总数）",L"",VideoSection::Information,true},
 {PowerDelta,VideoGroup::Monitor,L"电量指示器（发电 − 耗电）",L"",VideoSection::Information,true},
 {BuildTimeTooltip,VideoGroup::Monitor,L"原生建造栏提示框显示建造时间",L"",VideoSection::Information,true},
 {PsychicHints,VideoGroup::Monitor,L"敌方索敌信息",L"",VideoSection::Information,true},
 {CrateHints,VideoGroup::Monitor,L"箱子信息",L"",VideoSection::Information,true},
 {TextHints,VideoGroup::Monitor,L"战术文字提示",L"",VideoSection::Information,true},
 {IdleAircraft,VideoGroup::Monitor,L"空闲飞机指示器",L"",VideoSection::Information,true},
 {SuperweaponPanel,VideoGroup::Monitor,L"独立超级武器面板",L"",VideoSection::Information,true},
 {PassengerBubbles,VideoGroup::Monitor,L"载具载员气泡",L"",VideoSection::Information,true},
 {PassengerOwn,VideoGroup::Monitor,L"载员气泡包含己方",L"",VideoSection::Information},
 {PassengerAlways,VideoGroup::Monitor,L"载员气泡常驻显示",L"",VideoSection::Information},
 {DangerPassengers,VideoGroup::Monitor,L"危险载员常驻提示",L"",VideoSection::Information,true},
 {DetailedStatus,VideoGroup::Monitor,L"显示运行耗时",L"",VideoSection::Information},
 {TextMargin,VideoGroup::Monitor,L"文字提示上边距 / 像素",L"",VideoSection::Information},
 {ToastDuration,VideoGroup::Monitor,L"战术提示时长 / 毫秒",L"",VideoSection::Information}
};
// Native controls on their owning UI thread. Edits are posted to the existing
// setting owner; no synchronous cross-thread HWND access or duplicated INI.
class VideoSettings {
 static constexpr int Base=12000;HWND controls_[DisplayOptionCount]{},status_{};bool syncing_=false;
 static void metadata(HWND control,VideoSection section,bool header=false){
  if(!control)return;
  SetPropW(control,L"RA2.Console.Group",const_cast<wchar_t*>(videoSectionName(section)));
  SetPropW(control,L"RA2.Console.Collapsible",reinterpret_cast<HANDLE>(1));
  SetPropW(control,L"RA2.Console.CollapsedDefault",reinterpret_cast<HANDLE>(1));
  if(header)SetPropW(control,L"RA2.Console.GroupHeader",reinterpret_cast<HANDLE>(1));
 }
 static bool ready(DisplayOption option,const DisplayState&s){
  auto on=[&](DisplayOption o){return s.values[o]!=0;};
  if(option==PassengerOwn)return on(PassengerBubbles)||on(DangerPassengers);
  if(option==PassengerAlways)return on(PassengerBubbles);
  if(option==RenderUpgrade)return false; // Entry only; retain the saved value and all child settings.
  // Camera motion still consumes this preference while the main renderer is off.
  if(option==RenderHighRefresh)return true;
  if(option==RenderProcessing||option==RenderPhysical)return on(RenderUpgrade);
  if(option==RenderPalette||option==RenderPrecision||option==RenderMaterial||option==RenderDetail||option==RenderXbrz)return on(RenderUpgrade)&&on(RenderProcessing);
  if(option==RenderScale)return on(RenderUpgrade)&&on(RenderProcessing)&&on(RenderXbrz);
  if(option==WatchDanger||option==WatchKey1||option==WatchKey2||option==WatchKey3)return on(WatchPrimary)||on(WatchSecondary);
  if(option==WatchX||option==WatchY)return on(WatchSecondary);
  if(option==MapMode)return on(MapPrimary)||on(MapSecondary);
  if(option==MapScale||option==MapPosition)return on(MapPrimary);
  if(option==MapX||option==MapY||option==MapAlignX||option==MapAlignY)return on(MapPrimary)&&s.values[MapPosition]==3;
  if(option==MapSecondaryX||option==MapSecondaryY||option==MapSecondaryWidth)return on(MapSecondary);
  return true;
 }
public:
 template<class Create>int create(Create ctl,VideoGroup group,int y,int x,int width){
  auto title=ctl(L"STATIC",group==VideoGroup::Render?L"渲染与效果":L"副相机与小地图",0,x,y,width,30,0);modernui::heading(title);y+=48;
  for(const auto&e:videoEntries)if(e.group==group){const auto&type=displaySpecs[e.option].control;HWND h{};
   if(type==DisplayControl::Check)h=ctl(L"BUTTON",e.label,BS_AUTOCHECKBOX|WS_TABSTOP,x,y,width,28,Base+e.option);
   else{metadata(ctl(L"STATIC",e.label,0,x,y+3,width/2,25,0),e.section);const int left=x+width/2+10,w=width/2-10;
    if(type==DisplayControl::Choice){h=ctl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,left,y,w,190,Base+e.option);std::wstring choices=e.choices;size_t begin=0;do{auto end=choices.find(L'|',begin);auto item=choices.substr(begin,end-begin);SendMessageW(h,CB_ADDSTRING,0,(LPARAM)item.c_str());if(end==choices.npos)break;begin=end+1;}while(true);}
    else if(type==DisplayControl::Hotkey){h=ctl(HOTKEY_CLASSW,L"",WS_TABSTOP,left,y,w,28,Base+e.option);modernui::pressKey(h);}
    else{h=ctl(L"EDIT",L"",WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,left,y,w,28,Base+e.option);SendMessageW(h,EM_SETLIMITTEXT,5,0);}
   }
   metadata(h,e.section,e.groupHeader);controls_[e.option]=h;EnableWindow(h,FALSE);modernui::tip(h,e.option==OverlayToggleKey?L"统一显示或隐藏统计、小地图、副相机和战场信息。只在游戏前台且未聊天时生效；不暂停指令、不卸载 Hook。可在按键菜单中清除绑定。":e.label);y+=42;
  }
  if(group==VideoGroup::Render){status_=ctl(L"STATIC",L"正在同步渲染设置…",0,x,y,width,180,0);metadata(status_,VideoSection::Render);y+=194;modernui::tip(controls_[RenderGPU],L"GPU 不可用时回退 CPU；不同视角共用场景与绘制参数。状态显示设备、回退原因以及场景各阶段耗时。");}
  sync();return y;
 }
 void sync(bool force=false){DisplayState state;if(!readDisplaySettings(state))return;syncing_=true;
  for(int i=0;i<DisplayOptionCount;++i)if(auto h=controls_[i]){EnableWindow(h,ready(DisplayOption(i),state));const auto type=displaySpecs[i].control;
   if(type==DisplayControl::Check)SendMessageW(h,BM_SETCHECK,state.values[i]?BST_CHECKED:BST_UNCHECKED,0);
   else if(force||GetFocus()!=h){if(type==DisplayControl::Choice)SendMessageW(h,CB_SETCURSEL,state.values[i],0);else if(type==DisplayControl::Hotkey)SendMessageW(h,HKM_SETHOTKEY,state.values[i],0);else SetWindowTextW(h,std::to_wstring(state.values[i]).c_str());}
  }
  if(status_)SetWindowTextW(status_,state.renderStatus.empty()?L"等待游戏画面":state.renderStatus.c_str());syncing_=false;
 }
 bool command(WPARAM wp){int option=LOWORD(wp)-Base;if(option<0||option>=DisplayOptionCount||!controls_[option])return false;if(syncing_)return true;HWND h=controls_[option];if(!IsWindowEnabled(h)){sync(true);return true;}
  const auto type=displaySpecs[option].control;const int message=HIWORD(wp);int value=0;bool edit=false;
  if(type==DisplayControl::Check&&message==BN_CLICKED){value=SendMessageW(h,BM_GETCHECK,0,0)==BST_CHECKED;edit=true;}
  else if(type==DisplayControl::Choice&&message==CBN_SELCHANGE){value=int(SendMessageW(h,CB_GETCURSEL,0,0));edit=true;}
  else if(type==DisplayControl::Hotkey&&message==EN_CHANGE){value=int(SendMessageW(h,HKM_GETHOTKEY,0,0));edit=true;}
  else if(type==DisplayControl::Number&&message==EN_CHANGE){wchar_t text[32]{};GetWindowTextW(h,text,32);wchar_t*end=nullptr;long n=wcstol(text,&end,10);if(*text&&end&&!*end&&n>=displaySpecs[option].min&&n<=displaySpecs[option].max){value=int(n);edit=true;}}
  if(edit)requestDisplaySetting(option,value);else if(message==EN_KILLFOCUS)sync(true);return true;
 }
};
}
