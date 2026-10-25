#pragma once
#ifndef RA2_SECONDARY_DISPLAY_H
#define RA2_SECONDARY_DISPLAY_H
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace secondarydisplay {
constexpr DWORD RecheckMs=1000;
struct Target {
 HMONITOR monitor=nullptr;RECT work{};
 bool available()const{return monitor&&work.right>work.left&&work.bottom>work.top;}
};
// One resolver per owning UI thread. An extra display is always a different
// physical desktop monitor from the game's (or the supplied UI's) monitor.
// In particular, a single-monitor desktop never gets a duplicate on top of
// the game. Work coordinates remain signed for monitors left/above primary.
class Monitor {
 struct Scan {HMONITOR excluded=nullptr;RECT anchor{};bool excludedSeen=false;std::vector<Target> targets;};
 HWND window_=nullptr;HMONITOR excluded_=nullptr;DWORD sampledAt_=0;
 bool initialized_=false,dirty_=true;Target target_{};
 static BOOL CALLBACK collect(HMONITOR monitor,HDC,LPRECT,LPARAM parameter){
  auto&scan=*reinterpret_cast<Scan*>(parameter);if(!monitor)return TRUE;
  if(monitor==scan.excluded){scan.excludedSeen=true;return TRUE;}
  MONITORINFO info{};info.cbSize=sizeof info;
  if(!GetMonitorInfoW(monitor,&info))return TRUE;
  // Mirroring drivers can enumerate another handle for the same desktop
  // rectangle. Such a surface is not an additional physical destination.
  if(info.rcMonitor.left==scan.anchor.left&&info.rcMonitor.top==scan.anchor.top&&
     info.rcMonitor.right==scan.anchor.right&&info.rcMonitor.bottom==scan.anchor.bottom)return TRUE;
  RECT work{std::max(info.rcMonitor.left,info.rcWork.left),std::max(info.rcMonitor.top,info.rcWork.top),
            std::min(info.rcMonitor.right,info.rcWork.right),std::min(info.rcMonitor.bottom,info.rcWork.bottom)};
  Target candidate{monitor,work};if(candidate.available())scan.targets.push_back(candidate);return TRUE;
 }
 static Target sample(HMONITOR excluded){
  if(!excluded)return {};
  MONITORINFO anchor{};anchor.cbSize=sizeof anchor;
  if(!GetMonitorInfoW(excluded,&anchor)||anchor.rcMonitor.right<=anchor.rcMonitor.left||anchor.rcMonitor.bottom<=anchor.rcMonitor.top)return {};
  Scan scan;scan.excluded=excluded;scan.anchor=anchor.rcMonitor;
  // Do not publish an incomplete enumeration after a display driver failure.
  if(!EnumDisplayMonitors(nullptr,nullptr,collect,reinterpret_cast<LPARAM>(&scan))||!scan.excludedSeen||scan.targets.empty())return {};
  std::sort(scan.targets.begin(),scan.targets.end(),[](const Target&a,const Target&b){
   if(a.work.left!=b.work.left)return a.work.left<b.work.left;
   if(a.work.top!=b.work.top)return a.work.top<b.work.top;
   return reinterpret_cast<uintptr_t>(a.monitor)<reinterpret_cast<uintptr_t>(b.monitor);
  });
  return scan.targets.front();
 }
public:
 void invalidate(){dirty_=true;}
 Target get(HWND game,HWND fallback,DWORD now){
  HWND window=game&&IsWindow(game)?game:fallback&&IsWindow(fallback)?fallback:nullptr;
  HMONITOR excluded=window?MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST):nullptr;
  if(initialized_&&!dirty_&&window==window_&&excluded==excluded_&&DWORD(now-sampledAt_)<RecheckMs)return target_;
  initialized_=true;dirty_=false;window_=window;excluded_=excluded;sampledAt_=now;
  target_=sample(excluded);return target_;
 }
};
}
#endif
