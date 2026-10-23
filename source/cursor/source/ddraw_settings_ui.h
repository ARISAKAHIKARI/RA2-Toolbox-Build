#include "ddraw_config.h"
namespace ddrawsettings {
static bool fileRead(const std::wstring&path,std::string&data){
 HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER size{};bool ok=GetFileSizeEx(h,&size)&&size.QuadPart>=0&&size.QuadPart<=1024*1024;DWORD got=0;
 if(ok){data.resize(size_t(size.QuadPart));ok=ReadFile(h,data.data(),DWORD(data.size()),&got,nullptr)&&got==data.size();}CloseHandle(h);return ok;
}
static bool fileWrite(const std::wstring&path,const std::string&data){
 HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)return false;DWORD wrote=0;
 bool ok=WriteFile(h,data.data(),DWORD(data.size()),&wrote,nullptr)&&wrote==data.size()&&FlushFileBuffers(h);CloseHandle(h);return ok;
}
static bool exists(const std::wstring&p){return GetFileAttributesW(p.c_str())!=INVALID_FILE_ATTRIBUTES;}
static std::wstring directory(const std::wstring&p){auto i=p.find_last_of(L"\\/");return i==p.npos?L".":p.substr(0,i);}
static void message(const std::wstring&s){MessageBoxW(mainw,s.c_str(),L"DDraw 高刷设置",MB_OK|MB_ICONINFORMATION);}
static void configure(bool restore){
 std::wstring path,gamePath;ddrawconfig::Kind kind=ddrawconfig::Kind::Unknown;
 if(!restore){
  wchar_t exe[32768]{};DWORD length=32768;if(!pid||!cursorReader.imagePath(exe,&length)){message(L"请先启动游戏并启用工具箱显示，再读取已加载的 DDraw。");return;}gamePath=exe;
  HANDLE modules=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);if(modules!=INVALID_HANDLE_VALUE){MODULEENTRY32W m{sizeof m};if(Module32FirstW(modules,&m))do{
   auto found=ddrawconfig::identify([](uint32_t p,void*out,size_t n){return cursorReader.readFresh(p,out,n);},uint32_t(uintptr_t(m.modBaseAddr)),m.modBaseSize);
   if(found!=ddrawconfig::Kind::Unknown){if(kind!=ddrawconfig::Kind::Unknown){kind=ddrawconfig::Kind::Unknown;path.clear();break;}kind=found;path=directory(m.szExePath)+L"\\ddraw.ini";}
  }while(Module32NextW(modules,&m));CloseHandle(modules);}
  if(kind==ddrawconfig::Kind::Unknown){message(L"未识别到唯一的 CnC-DDraw 或 TS-DDraw 模块，未修改文件。请确认游戏实际使用的渲染器。");return;}
 }
 // CnC supports CNC_DDRAW_CONFIG_FILE; TS resolves against game CWD. A
 // file chooser makes the actual config explicit rather than guessing either.
 wchar_t selected[32768]{};if(!path.empty())wcsncpy(selected,path.c_str(),32767);
 OPENFILENAMEW pick{sizeof pick};pick.hwndOwner=mainw;pick.lpstrFilter=L"DDraw 配置\0*.ini\0所有文件\0*.*\0\0";pick.lpstrFile=selected;pick.nMaxFile=32768;
 pick.lpstrTitle=restore?L"选择需要恢复的 ddraw.ini":L"选择该游戏实际使用的 ddraw.ini（修改后须重启游戏）";pick.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
 if(!GetOpenFileNameW(&pick))return;path=selected;std::string original;if(!fileRead(path,original)){message(L"配置文件读取失败或超过 1 MB，未修改。");return;}
 std::wstring backup=path+L".ra2toolbox-refresh.bak",applied=path+L".ra2toolbox-refresh.applied";std::string next,last,saved;
 bool haveBackup=exists(backup),haveApplied=exists(applied);
 if(restore||haveBackup||haveApplied){
  if(!haveBackup||!haveApplied||!fileRead(backup,saved)||!fileRead(applied,last)||last!=original){message(L"配置已有其他修改，或恢复记录不完整；未覆盖当前文件。原始备份位置：\n"+backup);return;}
 }
 if(restore)next=saved;
 else{
  auto name=gamePath.substr(gamePath.find_last_of(L"\\/")+1);auto dot=name.find_last_of(L'.');if(dot!=name.npos)name.resize(dot);
  char narrow[1024]{};if(!WideCharToMultiByte(CP_ACP,0,name.c_str(),-1,narrow,sizeof narrow,nullptr,nullptr)){message(L"无法读取游戏配置节名称，未修改。");return;}
  auto edit=ddrawconfig::prepare(original,kind,narrow,configuredPollHz);if(!edit.error.empty()){message(L"配置格式、编码或目标刷新率不受支持，未修改。支持 ANSI / UTF-8 配置与 30–500 FPS。");return;}next=std::move(edit.result);
  if(next==original){message(L"所选 DDraw 配置已使用当前目标刷新率。若游戏已在运行，请重启游戏使配置生效。");return;}
 }
 wchar_t temporary[MAX_PATH]{};if(!GetTempFileNameW(directory(path).c_str(),L"r2f",0,temporary)){message(L"无法在配置目录创建临时文件，未修改。");return;}
 std::string current;bool ok=fileWrite(temporary,next)&&fileRead(path,current)&&current==original;
 if(ok)ok=ReplaceFileW(path.c_str(),temporary,(!restore&&!haveBackup)?backup.c_str():nullptr,0,nullptr,nullptr)!=FALSE;
 DeleteFileW(temporary);if(!ok){message(L"配置保存失败，或保存前已被其他程序修改。请检查文件权限；原有备份保留。");return;}
 if(restore){DeleteFileW(applied.c_str());DeleteFileW(backup.c_str());message(L"已恢复 DDraw 高刷修改前的配置。重启游戏后生效。\n"+path);}
 else if(!fileWrite(applied,next))message(L"配置已保存，但自动恢复记录写入失败。请保留原始备份，必要时手动恢复：\n"+backup);
 else message(std::wstring(L"已配置 ")+(kind==ddrawconfig::Kind::Cnc?L"CnC-DDraw":L"TS-DDraw")+L" 的呈现刷新率为 "+(configuredPollHz?std::to_wstring(configuredPollHz)+L" FPS":L"显示器刷新率")+L"。重启游戏后生效。\n游戏逻辑速度保持原设置；垂直同步仍可能限制到显示器刷新率。\n"+path);
}
}
