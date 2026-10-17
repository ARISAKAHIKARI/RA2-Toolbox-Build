#pragma once
struct PROCESSENTRY32W { unsigned long dwSize, cntUsage, th32ProcessID;void* th32DefaultHeapID;unsigned long th32ModuleID,cntThreads, th32ParentProcessID;long pcPriClassBase;unsigned long dwFlags; wchar_t szExeFile[260];};
extern "C" {HANDLE CreateToolhelp32Snapshot(DWORD,DWORD);BOOL Process32FirstW(HANDLE,PROCESSENTRY32W*);BOOL Process32NextW(HANDLE,PROCESSENTRY32W*);}
