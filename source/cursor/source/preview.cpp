#include <windows.h>
#include <shellapi.h>
#include <string>
int wmain(int argc,wchar_t** argv){
 wchar_t module[32768];GetModuleFileNameW(NULL,module,32768);
 std::wstring root=module;root=root.substr(0,root.find_last_of(L"\\"));
 std::wstring page=argc>1?argv[1]:root+L"\\assets\\preview.html";
 auto result=ShellExecuteW(NULL,L"open",page.c_str(),NULL,NULL,SW_SHOWNORMAL);
 if((INT_PTR)result<=32){MessageBoxW(NULL,L"无法打开预览，请完整解压后打开 assets\\preview.html。",L"动画预览",MB_OK|MB_ICONERROR);return 1;}
 return 0;
}
