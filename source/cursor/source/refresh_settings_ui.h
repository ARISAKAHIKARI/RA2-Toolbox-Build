#ifdef RA2_UNIFIED
static HWND refreshModeBox,refreshStateLabel,shroudModeBox;static bool refreshingSuiteControls=false;
static void syncRefreshControls(){
 suite::DisplayState state;if(!suite::readDisplaySettings(state)||!refreshModeBox)return;
 refreshingSuiteControls=true;EnableWindow(refreshModeBox,TRUE);
 SendMessageW(refreshModeBox,CB_SETCURSEL,state.values[suite::RefreshMode],0);
 if(shroudModeBox){EnableWindow(shroudModeBox,TRUE);SendMessageW(shroudModeBox,CB_SETCURSEL,state.values[suite::ShroudStrategy],0);}
 bool scene=state.values[suite::RefreshMode]!=0;
 std::wstring text=scene?L"等待游戏前台就绪":L"仅光标 · 使用上方目标刷新率";
 if(scene&&!state.assistEnabled)text=L"画面高刷已暂停";
 else if(scene&&state.refreshStatus<0)text=L"当前游戏使用原版画面刷新";
 else if(scene&&state.refreshStatus==1){
  wchar_t value[160];swprintf(value,160,L"绘制调用 %.1f Hz（非实测屏幕 FPS）   ·   最长间隔 %.1f ms",state.rateMilli/1000.0,state.gapUs/1000.0);text=value;
 }
 SetWindowTextW(refreshStateLabel,text.c_str());refreshingSuiteControls=false;
}
static bool refreshSettingCommand(WPARAM wp,LPARAM lp){
 if(refreshingSuiteControls)return false;HWND control=(HWND)lp;
 if(control==shroudModeBox&&HIWORD(wp)==CBN_SELCHANGE){suite::requestDisplaySetting(suite::ShroudStrategy,int(SendMessageW(control,CB_GETCURSEL,0,0)));return true;}
 if(control==refreshModeBox&&HIWORD(wp)==CBN_SELCHANGE){suite::requestDisplaySetting(suite::RefreshMode,int(SendMessageW(control,CB_GETCURSEL,0,0)));return true;}
 return false;
}
#endif
