#pragma once
#include <stddef.h>
#include <stdint.h>
using DWORD=uint32_t; using LONG=int32_t; using BOOL=int; using HANDLE=void*;using HWND=void*;using LPARAM=long;using SIZE_T=size_t;using WCHAR=wchar_t;using LPCVOID=const void*;
struct RECT {long left,top,right,bottom;};
struct MEMORY_BASIC_INFORMATION { void* BaseAddress;void* AllocationBase;DWORD AllocationProtect;SIZE_T RegionSize;DWORD State;DWORD Protect;DWORD Type;};
#define FALSE 0
#define TRUE 1
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#define PROCESS_QUERY_INFORMATION 0x400
#define PROCESS_VM_READ 0x10
#define TH32CS_SNAPPROCESS 2
#define STILL_ACTIVE 259
#define ERROR_INVALID_PARAMETER 87
#define ERROR_INVALID_HANDLE 6
#define ERROR_PARTIAL_COPY 299
#define ERROR_ACCESS_DENIED 5
#define GW_OWNER 4
#define GWL_EXSTYLE (-20)
#define WS_EX_TOOLWINDOW 0x00000080L
extern "C" { HANDLE OpenProcess(DWORD,BOOL,DWORD); BOOL CloseHandle(HANDLE); DWORD GetLastError(); BOOL GetExitCodeProcess(HANDLE,DWORD*); BOOL ReadProcessMemory(HANDLE,LPCVOID,void*,SIZE_T,SIZE_T*); SIZE_T VirtualQueryEx(HANDLE,LPCVOID,MEMORY_BASIC_INFORMATION*,SIZE_T);DWORD GetCurrentProcessId();BOOL EnumWindows(BOOL(*)(HWND,LPARAM),LPARAM); BOOL GetClientRect(HWND,RECT*); DWORD GetWindowThreadProcessId(HWND,DWORD*); BOOL IsWindowVisible(HWND); BOOL IsIconic(HWND); LONG GetWindowLongW(HWND,int); HWND GetWindow(HWND,unsigned);}
extern "C" BOOL QueryFullProcessImageNameW(HANDLE,DWORD,wchar_t*,DWORD*);
#define CALLBACK
#define ERROR_SUCCESS 0
