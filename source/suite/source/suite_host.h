#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../../shared/source/modern_ui.h"
#include "display_settings.h"
#include "settings_categories.h"
#include "frontend_panes.h"
#include "../../shared/source/overlay_visibility.h"
namespace suite {
inline bool overlaysVisible(){return overlayvisibility::visible();}
constexpr UINT PaneLayout=WM_APP+80,ModuleEnable=WM_APP+81,CloseCheck=WM_APP+82;
constexpr UINT ApplyDisplaySetting=WM_APP+83,DisplaySettingsChanged=WM_APP+84,RefreshRateChanged=WM_APP+85,SelectCategory=WM_APP+86;
constexpr WPARAM CloseToken=0x52413253;
using Layout=frontendpanes::Layout;
inline frontendpanes::Host frontend;
inline bool managedPane(HWND w){return frontend.managed(w);}
HWND parent();
bool enabled(int module);
void registerPane(int module,HWND pane);
void reportEnabled(int module,bool value);
void requestClose();
void closeDecision(bool allow);
void setStatsVisible(bool value);
bool statsVisible();
void publishHardwareCursor(bool active);
bool hardwareCursorActive();
// Cursor owns the common target, including automatic monitor-rate resolution.
void publishRefreshRate(unsigned hz);
unsigned refreshRate();
void publishDisplaySettings(const DisplayState& state);
bool readDisplaySettings(DisplayState& state);
bool requestDisplaySetting(int option,int value);
// Executed on each module's UI thread. A single latest-state mailbox replaces
// per-resize heap payloads; no child/owner input-queue attachment is involved.
inline bool layoutMessage(HWND w,UINT m,LPARAM lp){
 (void)lp;return frontend.apply(w,m,modernui::DpiMessage);
}
}
