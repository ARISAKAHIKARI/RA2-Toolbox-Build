#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../../shared/source/modern_ui.h"
#include "display_settings.h"
namespace suite {
constexpr UINT PaneLayout=WM_APP+80,ModuleEnable=WM_APP+81,CloseCheck=WM_APP+82;
constexpr UINT ApplyDisplaySetting=WM_APP+83,DisplaySettingsChanged=WM_APP+84;
constexpr WPARAM CloseToken=0x52413253;
struct Layout {RECT rect;bool visible;};
HWND parent();
bool enabled(int module);
void registerPane(int module,HWND pane);
void reportEnabled(int module,bool value);
void requestClose();
void closeDecision(bool allow);
void setStatsVisible(bool value);
bool statsVisible();
void publishDisplaySettings(const DisplayState& state);
bool readDisplaySettings(DisplayState& state);
bool requestDisplaySetting(int option,int value);
// Executed on each module's UI thread. Layout requests own their payload.
inline bool layoutMessage(HWND w,UINT m,LPARAM lp){
 if(m!=PaneLayout)return false;auto p=(Layout*)lp;modernui::styleChildren(w);
 SetWindowPos(w,nullptr,p->rect.left,p->rect.top,p->rect.right-p->rect.left,p->rect.bottom-p->rect.top,SWP_NOZORDER|SWP_NOACTIVATE);
 ShowWindow(w,p->visible?SW_SHOWNOACTIVATE:SW_HIDE);delete p;return true;
}
}
