#pragma once
#include <windows.h>
namespace settingsdialog {
// A managed settings pane can be hidden or DWM-cloaked independently of the
// shell. Its owned dialogs would inherit that concealment. Keep these dialogs
// on their caller's UI thread with no owner; never attach them to another
// thread's shell. TASKMODAL disables only that thread's top-level windows.
inline int message(HWND localOwner,bool managed,const wchar_t*text,const wchar_t*title,UINT flags){
 return MessageBoxW(managed?nullptr:localOwner,text,title,
  flags|(managed?(MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST):0));
}
}
