#pragma once
#include <stddef.h>
#include <stdint.h>
#include <cwchar>
using DWORD=uint32_t; using LONG=int32_t; using BOOL=int; using HANDLE=void*;using HMODULE=void*;using HWND=void*;using LPARAM=long;using SIZE_T=size_t;using WCHAR=wchar_t;using LPCVOID=const void*;
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
#define ERROR_INVALID_DATA 13
#define ERROR_GEN_FAILURE 31
#define ERROR_NO_MORE_FILES 18
#define ERROR_PROC_NOT_FOUND 127
#define ERROR_MOD_NOT_FOUND 126
#define ERROR_BAD_LENGTH 24
#define ERROR_WRITE_FAULT 29
#define ERROR_NOT_FOUND 1168
#define ERROR_TOO_MANY_NAMES 68
#define ERROR_NOT_ENOUGH_MEMORY 8
#define ERROR_RESOURCE_DATA_NOT_FOUND 1812
#define ERROR_RESOURCE_TYPE_NOT_FOUND 1813
#define ERROR_RESOURCE_NAME_NOT_FOUND 1814
#define ERROR_RESOURCE_LANG_NOT_FOUND 1815
#define SYNCHRONIZE 0x100000
#define WAIT_TIMEOUT 258
#define WAIT_OBJECT_0 0
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x800
#define WINAPI
#define GW_OWNER 4
#define GWL_EXSTYLE (-20)
#define WS_EX_TOOLWINDOW 0x00000080L
extern "C" { HANDLE OpenProcess(DWORD,BOOL,DWORD); BOOL CloseHandle(HANDLE); DWORD GetLastError(); BOOL GetExitCodeProcess(HANDLE,DWORD*); BOOL ReadProcessMemory(HANDLE,LPCVOID,void*,SIZE_T,SIZE_T*); SIZE_T VirtualQueryEx(HANDLE,LPCVOID,MEMORY_BASIC_INFORMATION*,SIZE_T);DWORD GetCurrentProcessId();BOOL EnumWindows(BOOL(*)(HWND,LPARAM),LPARAM); BOOL GetClientRect(HWND,RECT*); DWORD GetWindowThreadProcessId(HWND,DWORD*); BOOL IsWindowVisible(HWND); BOOL IsIconic(HWND); LONG GetWindowLongW(HWND,int); HWND GetWindow(HWND,unsigned);}
extern "C" BOOL QueryFullProcessImageNameW(HANDLE,DWORD,wchar_t*,DWORD*);
#define CALLBACK
#define ERROR_SUCCESS 0
struct LARGE_INTEGER {int64_t QuadPart;};
struct FILETIME {DWORD dwLowDateTime,dwHighDateTime;};
constexpr DWORD GENERIC_READ=0x80000000,FILE_SHARE_READ=1,FILE_SHARE_WRITE=2,FILE_SHARE_DELETE=4,OPEN_EXISTING=3,FILE_ATTRIBUTE_NORMAL=128,FILE_BEGIN=0;
#ifdef WINMOCK_IMAGE_FILES
extern "C" {
HANDLE CreateFileW(const wchar_t*,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
BOOL GetFileSizeEx(HANDLE,LARGE_INTEGER*);BOOL SetFilePointerEx(HANDLE,LARGE_INTEGER,LARGE_INTEGER*,DWORD);BOOL ReadFile(HANDLE,void*,DWORD,DWORD*,void*);
BOOL GetProcessTimes(HANDLE,FILETIME*,FILETIME*,FILETIME*,FILETIME*);DWORD GetTickCount();
HWND GetForegroundWindow();BOOL EnumChildWindows(HWND,BOOL(*)(HWND,LPARAM),LPARAM);
}
#else
inline HANDLE CreateFileW(const wchar_t*,DWORD,DWORD,void*,DWORD,DWORD,HANDLE){return INVALID_HANDLE_VALUE;}
inline BOOL GetFileSizeEx(HANDLE,LARGE_INTEGER*){return FALSE;}
inline BOOL SetFilePointerEx(HANDLE,LARGE_INTEGER,LARGE_INTEGER*,DWORD){return FALSE;}
inline BOOL ReadFile(HANDLE,void*,DWORD,DWORD*,void*){return FALSE;}
inline BOOL GetProcessTimes(HANDLE,FILETIME*,FILETIME*,FILETIME*,FILETIME*){return FALSE;}
inline DWORD GetTickCount(){return 1;}
inline HWND GetForegroundWindow(){return nullptr;}
inline BOOL EnumChildWindows(HWND,BOOL(*)(HWND,LPARAM),LPARAM){return TRUE;}
#endif
#ifdef WINMOCK_VERSION_FILES
extern "C" {HMODULE LoadLibraryExW(const wchar_t*,HANDLE,DWORD);void* GetProcAddress(HMODULE,const char*);}
#else
inline HMODULE LoadLibraryExW(const wchar_t*,HANDLE,DWORD){return nullptr;}
inline void* GetProcAddress(HMODULE,const char*){return nullptr;}
#endif
#ifdef WINMOCK_TARGET_PROFILE
extern "C" {DWORD GetModuleFileNameW(HMODULE,wchar_t*,DWORD);DWORD GetPrivateProfileStringW(const wchar_t*,const wchar_t*,const wchar_t*,wchar_t*,DWORD,const wchar_t*);BOOL WritePrivateProfileSectionW(const wchar_t*,const wchar_t*,const wchar_t*);}
#else
inline DWORD GetModuleFileNameW(HMODULE,wchar_t*out,DWORD count){const wchar_t* text=L"C:\\Fixture\\RA2Toolbox.exe";if(count<=std::wcslen(text))return 0;std::wcscpy(out,text);return DWORD(std::wcslen(text));}
inline DWORD GetPrivateProfileStringW(const wchar_t*,const wchar_t*,const wchar_t* fallback,wchar_t*out,DWORD count,const wchar_t*){std::wcsncpy(out,fallback,count);out[count-1]=0;return DWORD(std::wcslen(out));}
inline BOOL WritePrivateProfileSectionW(const wchar_t*,const wchar_t*,const wchar_t*){return TRUE;}
#endif
#ifdef WINMOCK_DESCRIPTION_WORKERS
extern "C" {HANDLE CreateThread(void*,size_t,DWORD(*)(void*),void*,DWORD,DWORD*);DWORD WaitForSingleObject(HANDLE,DWORD);}
#else
inline HANDLE CreateThread(void*,size_t,DWORD(*)(void*),void*,DWORD,DWORD*){return nullptr;}
inline DWORD WaitForSingleObject(HANDLE,DWORD){return WAIT_TIMEOUT;}
#endif
