#ifdef RA2_UNIFIED
#include "../../suite/source/video_settings_ui.h"
static suite::VideoSettings monitorVideoSettings;
static bool syncingUnitSettings=false;static HWND unitSettingControls[suite::DisplayOptionCount]{},unitInfoRateStatus;
static int statsInformationFPS();
static void syncUnitInformationRate(){if(unitInfoRateStatus)SetWindowTextW(unitInfoRateStatus,(L"当前："+std::to_wstring(statsInformationFPS())+L" Hz").c_str());}
static void syncUnitSettings(bool force=false){
 monitorVideoSettings.sync(force);suite::DisplayState state;if(!suite::readDisplaySettings(state))return;
 syncingUnitSettings=true;
 for(int i=0;i<suite::DisplayOptionCount;++i)if(auto control=unitSettingControls[i]){
  EnableWindow(control,TRUE);
  if(suite::displaySpecs[i].control==suite::DisplayControl::Check)SendMessageW(control,BM_SETCHECK,state.values[i]?BST_CHECKED:BST_UNCHECKED,0);
  else if(force||GetFocus()!=control)SetWindowTextW(control,std::to_wstring(state.values[i]).c_str());
 }
 EnableWindow(unitSettingControls[suite::AllTransports],state.values[suite::Capacity]!=0);
 EnableWindow(unitSettingControls[suite::InfoFPS],state.values[suite::InfoAdaptive]==0);
 syncUnitInformationRate();
 syncingUnitSettings=false;
}
template<class Create> static void createUnitSettings(Create ctl){
 auto title=ctl(L"STATIC",L"单位状态",0,354,16,326,28,0);modernui::heading(title);
 auto toggle=[&](suite::DisplayOption option,const wchar_t* text,int y,int x=354){
  auto h=ctl(L"BUTTON",text,BS_AUTOCHECKBOX|WS_TABSTOP,x,y,680-x,26,600+option);unitSettingControls[option]=h;EnableWindow(h,FALSE);
 };
 toggle(suite::EnemyMarkers,L"敌方单位提示",58);
 toggle(suite::Capacity,L"载具载员与容量",96);
 toggle(suite::AllTransports,L"同时显示未选中的载具",130,374);
 modernui::tip(unitSettingControls[suite::AllTransports],L"需先开启“载具载员与容量”。关闭父功能时保留此选项，重新开启后恢复使用。");
 toggle(suite::Iron,L"铁幕 / 力场剩余时间",174);
 toggle(suite::Frozen,L"冷冻剩余时间",212);
 toggle(suite::Parasite,L"蜘蛛寄生标志",250);
 toggle(suite::CrateBuffs,L"火力 / 防御 / 速度强化",288);
 toggle(suite::PassengerBubbles,L"敌方载员气泡",326);
 toggle(suite::DangerPassengers,L"危险载员常驻提示",364);
 modernui::tip(unitSettingControls[suite::PassengerBubbles],L"悬停或选中敌方载具时，在正上方显示乘员类型与数量。");
 modernui::tip(unitSettingControls[suite::DangerPassengers],L"敌方载具内含工程师、间谍或爆破单位时常驻显示载员气泡。");
 auto number=[&](suite::DisplayOption option,const wchar_t* text,int y,const wchar_t* unit){
  ctl(L"STATIC",text,0,354,y+3,178,23,0);
  auto h=ctl(L"EDIT",L"",WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,538,y,74,26,600+option);unitSettingControls[option]=h;SendMessageW(h,EM_SETLIMITTEXT,3,0);EnableWindow(h,FALSE);
  ctl(L"STATIC",unit,0,624,y+3,56,23,0);
 };
 number(suite::LabelScale,L"状态文字缩放",420,L"%");
 toggle(suite::InfoAdaptive,L"自适应屏幕刷新率",462);
 number(suite::InfoFPS,L"手动信息刷新率",500,L"Hz");
 unitInfoRateStatus=ctl(L"STATIC",L"当前：— Hz",0,354,534,326,23,614);
 modernui::tip(unitSettingControls[suite::InfoAdaptive],L"跟随游戏所在屏幕的名义刷新率；未运行游戏时使用工具箱所在屏幕。最高 500 Hz，读取失败时使用 60 Hz。");
 modernui::tip(unitSettingControls[suite::InfoFPS],L"手动目标为 5–500 Hz。启用自适应时保留此值，关闭自适应后恢复使用。");
 monitorVideoSettings.create(ctl,suite::VideoGroup::Monitor,606,14,666);
 syncUnitSettings();
}
static bool unitSettingCommand(WPARAM wp){
 if(monitorVideoSettings.command(wp))return true;
 int option=LOWORD(wp)-600;if(option<0||option>=suite::DisplayOptionCount||!unitSettingControls[option])return false;
 if(syncingUnitSettings)return true;
 auto h=unitSettingControls[option];auto type=suite::displaySpecs[option].control;
 if(!IsWindowEnabled(h)){syncUnitSettings(true);return true;}
 if(type==suite::DisplayControl::Check&&HIWORD(wp)==BN_CLICKED)suite::requestDisplaySetting(option,SendMessageW(h,BM_GETCHECK,0,0)==BST_CHECKED);
 else if(type==suite::DisplayControl::Number&&HIWORD(wp)==EN_CHANGE){BOOL ok=FALSE;int value=GetDlgItemInt(ui,LOWORD(wp),&ok,FALSE);if(ok)suite::requestDisplaySetting(option,value);}
 else if(HIWORD(wp)==EN_KILLFOCUS)syncUnitSettings(true);return true;
}
#endif
