#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <cstdlib>
#include "physical_presentation.h"

// Read-only display probe. It does not alter process DPI awareness, monitor
// modes, DirectDraw cooperative level, game HWND or game executable pages.
namespace physicalpresentation {
inline Result inspect(HWND game,int gameW,int gameH,POINT clientOrigin,const RECT&client){
 Observation o;
 o.sourceW=gameW;o.sourceH=gameH;
 o.clientW=client.right-client.left;o.clientH=client.bottom-client.top;
 if(!game||!IsWindow(game)||o.clientW<1||o.clientH<1){
  o.captureFailed=true;return classify(o);
 }
 HMONITOR monitor=MonitorFromWindow(game,MONITOR_DEFAULTTONEAREST);
 MONITORINFOEXW info{};info.cbSize=sizeof(info);
 if(!monitor||!GetMonitorInfoW(monitor,&info)){
  o.captureFailed=true;return classify(o);
 }
 o.monitorW=info.rcMonitor.right-info.rcMonitor.left;
 o.monitorH=info.rcMonitor.bottom-info.rcMonitor.top;
 o.coversMonitor=
  std::abs(int64_t(clientOrigin.x)-info.rcMonitor.left)<=1&&
  std::abs(int64_t(clientOrigin.y)-info.rcMonitor.top)<=1&&
  std::abs(int64_t(clientOrigin.x)+o.clientW-info.rcMonitor.right)<=1&&
  std::abs(int64_t(clientOrigin.y)+o.clientH-info.rcMonitor.bottom)<=1;
 DEVMODEW current{};current.dmSize=sizeof(current);
 if(!EnumDisplaySettingsExW(info.szDevice,ENUM_CURRENT_SETTINGS,&current,0)){
  o.captureFailed=true;return classify(o);
 }
 o.currentW=int(current.dmPelsWidth);o.currentH=int(current.dmPelsHeight);
 DEVMODEW configured{};configured.dmSize=sizeof(configured);
 if(EnumDisplaySettingsExW(info.szDevice,ENUM_REGISTRY_SETTINGS,&configured,0)){
  o.registryW=int(configured.dmPelsWidth);o.registryH=int(configured.dmPelsHeight);
 }
 return classify(o);
}
}
