#ifdef RA2_UNIFIED
static HWND refreshModeBox,refreshRateBox,refreshStateLabel;static bool refreshingSuiteControls=false;
static void syncRefreshControls(bool force=false){
 suite::DisplayState state;if(!suite::readDisplaySettings(state)||!refreshModeBox)return;
 refreshingSuiteControls=true;EnableWindow(refreshModeBox,TRUE);
 SendMessageW(refreshModeBox,CB_SETCURSEL,state.values[suite::RefreshMode],0);
 bool scene=state.values[suite::RefreshMode]!=0;EnableWindow(refreshRateBox,scene);
 if(force||GetFocus()!=refreshRateBox)SetWindowTextW(refreshRateBox,std::to_wstring(state.values[suite::RefreshHz]).c_str());
 std::wstring text=scene?L"等待游戏前台就绪":L"仅光标 · 使用上方光标目标帧率";
 if(scene&&!state.assistEnabled)text=L"画面高刷已暂停";
 else if(scene&&state.refreshStatus<0)text=L"当前游戏使用原版画面刷新";
 else if(scene&&state.refreshStatus==1){
  wchar_t value[160];swprintf(value,160,L"补充绘制 %.1f Hz   ·   最长间隔 %.1f ms",state.rateMilli/1000.0,state.gapUs/1000.0);text=value;
 }
 SetWindowTextW(refreshStateLabel,text.c_str());refreshingSuiteControls=false;
}
static bool refreshSettingCommand(WPARAM wp,LPARAM lp){
 if(refreshingSuiteControls)return false;HWND control=(HWND)lp;
 if(control==refreshModeBox&&HIWORD(wp)==CBN_SELCHANGE){suite::requestDisplaySetting(suite::RefreshMode,int(SendMessageW(control,CB_GETCURSEL,0,0)));return true;}
 if(control==refreshRateBox&&HIWORD(wp)==EN_CHANGE){BOOL ok=FALSE;int rate=GetDlgItemInt(mainw,156,&ok,FALSE);if(ok)suite::requestDisplaySetting(suite::RefreshHz,rate);return true;}
 if(control==refreshRateBox&&HIWORD(wp)==EN_KILLFOCUS){syncRefreshControls(true);return true;}return false;
}
#endif
