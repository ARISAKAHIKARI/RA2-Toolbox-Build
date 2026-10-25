#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cstdint>

namespace monitorrefresh {
constexpr int FallbackHz=60,MinHz=5,MaxHz=500;
constexpr DWORD RecheckMs=1000;
inline int boundedHz(uint32_t hz){return hz<MinHz?FallbackHz:int(std::min(hz,uint32_t(MaxHz)));}
// Integer microseconds retain sub-millisecond spacing on high-refresh screens.
inline uint64_t periodMicros(int hz){return 1000000ull/uint64_t(hz<MinHz?FallbackHz:std::min(hz,MaxHz));}

// Owned by one UI thread. The caller supplies the game window first and its
// own visible UI/overlay as fallback; this never inherits the cursor's target.
class Tracker {
 HWND target_=nullptr;HMONITOR monitor_=nullptr;DWORD sampledAt_=0;
 int rate_=FallbackHz;bool initialized_=false,dirty_=true;
 static int sample(HMONITOR monitor){
  if(!monitor)return FallbackHz;
  MONITORINFOEXW info{};info.cbSize=sizeof(info);
  if(!GetMonitorInfoW(monitor,reinterpret_cast<LPMONITORINFO>(&info)))return FallbackHz;
  DEVMODEW mode{};mode.dmSize=sizeof(mode);
  if(!EnumDisplaySettingsExW(info.szDevice,ENUM_CURRENT_SETTINGS,&mode,0)||!(mode.dmFields&DM_DISPLAYFREQUENCY))return FallbackHz;
  // DEVMODE's 0/1 mean hardware default, not a known frequency in Hz.
  return boundedHz(mode.dmDisplayFrequency);
 }
public:
 int rate()const{return rate_;}
 void invalidate(){dirty_=true;}
 int update(HWND game,HWND fallback,DWORD now,bool force=false){
  HWND target=game&&IsWindow(game)?game:fallback&&IsWindow(fallback)?fallback:nullptr;
  // Largest window/monitor intersection, or nearest monitor when off-screen.
  // Checking the handle each control tick catches same-HWND monitor moves.
  HMONITOR monitor=target?MonitorFromWindow(target,MONITOR_DEFAULTTONEAREST):nullptr;
  if(initialized_&&!dirty_&&!force&&target==target_&&monitor==monitor_&&DWORD(now-sampledAt_)<RecheckMs)return rate_;
  initialized_=true;dirty_=false;target_=target;monitor_=monitor;sampledAt_=now;
  // Recheck even an unchanged monitor once per second: a frequency-only mode
  // change need not produce the resolution-change notification we receive.
  rate_=sample(monitor);return rate_;
 }
};
}
