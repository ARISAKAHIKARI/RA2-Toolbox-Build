#pragma once
#include <cstdint>
#include <cstddef>
using BOOL=int;using DWORD=uint32_t;using LONG=int32_t;using UINT=unsigned;using HANDLE=void*;using HWND=void*;using HMODULE=void*;using HINSTANCE=void*;using HMONITOR=void*;using HDC=void*;using HCURSOR=void*;using WPARAM=uintptr_t;using LPARAM=intptr_t;using LRESULT=intptr_t;
struct POINT{LONG x,y;};struct RECT{LONG left,top,right,bottom;};struct SIZE{LONG cx,cy;};
struct BLENDFUNCTION{unsigned char BlendOp,BlendFlags,SourceConstantAlpha,AlphaFormat;};
struct MONITORINFO{DWORD cbSize;RECT rcMonitor{},rcWork{};DWORD flags=0;};
struct CURSORINFO{DWORD cbSize,flags=0;HCURSOR hCursor=nullptr;POINT ptScreenPos{};};
struct CWPRETSTRUCT{LRESULT lResult=0;LPARAM lParam=0;WPARAM wParam=0;UINT message=0;HWND hwnd=nullptr;};
#define WINAPI
constexpr BOOL FALSE=0,TRUE=1;
constexpr DWORD ERROR_INVALID_DATA=13,ERROR_CALL_NOT_IMPLEMENTED=120;
constexpr DWORD WS_EX_LAYERED=1,WS_EX_TRANSPARENT=2,WS_EX_NOACTIVATE=4,WS_EX_TOOLWINDOW=8,WS_EX_TOPMOST=16,WS_POPUP=32,LWA_ALPHA=2;
constexpr UINT SWP_NOSIZE=1,SWP_NOACTIVATE=2,SWP_SHOWWINDOW=4,SWP_NOMOVE=8,ULW_ALPHA=2;
constexpr int AC_SRC_OVER=0,AC_SRC_ALPHA=1,MONITOR_DEFAULTTONEAREST=2,SW_HIDE=0;
constexpr UINT WM_SETCURSOR=0x20,CURSOR_SHOWING=1;constexpr int HTCLIENT=1;
inline const wchar_t* IDC_ARROW=L"arrow";
inline unsigned LOWORD(LPARAM value){return unsigned(uintptr_t(value)&65535u);}
inline LONG InterlockedExchange(volatile LONG* pointer,LONG value){const LONG old=*pointer;*pointer=value;return old;}
inline LONG InterlockedIncrement(volatile LONG* pointer){return ++*pointer;}
inline void MemoryBarrier(){}
HMODULE GetModuleHandleW(const wchar_t*);void* GetProcAddress(HMODULE,const char*);
BOOL GetCursorPos(POINT*);BOOL GetClientRect(HWND,RECT*);BOOL ClientToScreen(HWND,POINT*);BOOL GetClipCursor(RECT*);
DWORD GetLastError();void SetLastError(DWORD);DWORD GetTickCount();
BOOL IsWindow(HWND);BOOL IsWindowVisible(HWND);BOOL IsIconic(HWND);void SetRectEmpty(RECT*);BOOL ShowWindow(HWND,int);
HWND CreateWindowExW(DWORD,const wchar_t*,const wchar_t*,DWORD,int,int,int,int,HWND,HANDLE,HINSTANCE,void*);
BOOL DestroyWindow(HWND);BOOL SetLayeredWindowAttributes(HWND,DWORD,unsigned char,DWORD);
BOOL GetWindowRect(HWND,RECT*);BOOL UpdateLayeredWindow(HWND,HDC,const POINT*,const SIZE*,HDC,const POINT*,DWORD,const BLENDFUNCTION*,DWORD);
HMONITOR MonitorFromWindow(HWND,DWORD);BOOL GetMonitorInfoW(HMONITOR,MONITORINFO*);
HCURSOR GetCursor();HCURSOR SetCursor(HCURSOR);HCURSOR LoadCursorW(HINSTANCE,const wchar_t*);
BOOL GetCursorInfo(CURSORINFO*);int ShowCursor(BOOL);
