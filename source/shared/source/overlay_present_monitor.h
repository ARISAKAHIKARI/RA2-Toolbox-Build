#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "overlay_present_rate.h"
namespace overlaypresentrate {
// Owned only by the compositor thread. Monitor handle checks are cheap; the
// current-mode query is throttled, with immediate window/monitor-change repair.
class Monitor {
 HWND target_=nullptr;HMONITOR monitor_=nullptr;DWORD sampledAt_=0;
 int confirmed_=0;bool initialized_=false,dirty_=true;
 static int sample(HMONITOR monitor){
  if(!monitor)return 0;
  MONITORINFOEXW info{};info.cbSize=sizeof(info);
  if(!GetMonitorInfoW(monitor,reinterpret_cast<LPMONITORINFO>(&info)))return 0;
  DEVMODEW mode{};mode.dmSize=sizeof(mode);
  if(!EnumDisplaySettingsExW(info.szDevice,ENUM_CURRENT_SETTINGS,&mode,0)||!(mode.dmFields&DM_DISPLAYFREQUENCY))return 0;
  return confirmed(mode.dmDisplayFrequency);
 }
public:
 int rate()const{return confirmed_;}
 void invalidate(){dirty_=true;}
 int update(HWND window,DWORD now){
  HWND target=window&&IsWindow(window)?window:nullptr;
  HMONITOR monitor=target?MonitorFromWindow(target,MONITOR_DEFAULTTONEAREST):nullptr;
  if(initialized_&&!dirty_&&target==target_&&monitor==monitor_&&DWORD(now-sampledAt_)<1000)return confirmed_;
  initialized_=true;dirty_=false;target_=target;monitor_=monitor;sampledAt_=now;
  confirmed_=sample(monitor);return confirmed_;
 }
};
}
