#pragma once
#include <algorithm>
#include <atomic>
#include <cwctype>
#include <memory>
#include <new>
#include <string>
#include <vector>
#include "../../external_reader/source/process_target_selection.h"

// Included after pageControl(). This page edits discovery policy only. The
// normal Host lifecycle observes its revision and owns any connection change.
static constexpr int GameProcessSearch=12400,GameProcessSaveSearch=12401,
 GameProcessRunning=12402,GameProcessRefresh=12403,GameProcessRemember=12404,
 GameProcessTarget=12405,GameProcessAdd=12406,GameProcessRemembered=12407,
 GameProcessRemove=12408,GameProcessClearPreferred=12409,
 GameProcessConnection=12410,GameProcessFeedback=12411;
struct GameProcessSelectionJob {
 std::vector<external::targetselection::ProcessInfo> processes;
 DWORD error=0;
 std::atomic<bool> done{false};
 uint64_t generation=0;
};
static HWND gameProcessSearchEdit,gameProcessRunningList,gameProcessRefreshButton,
 gameProcessRememberButton,gameProcessTargetEdit,gameProcessRememberedList,
 gameProcessRemoveButton,gameProcessClearButton,gameProcessConnectionText,
 gameProcessFeedbackText;
static std::vector<external::targetselection::ProcessInfo> gameProcessCandidates;
static std::vector<size_t> gameProcessVisible;
static std::vector<std::wstring> gameProcessRememberedPrograms;
static std::shared_ptr<GameProcessSelectionJob> gameProcessSelectionJob;
static uint64_t gameProcessSelectionGeneration=0,gameProcessSelectionPublished=0,
 gameProcessSelectionPolicyRevision=~uint64_t(0);
static DWORD gameProcessSelectionUIAt=0;
static std::wstring gameProcessSelectionFeedback,gameProcessSelectionConnection;
static bool gameProcessSelectionClosing=false,gameProcessSelectionUpdating=false,gameProcessSelectionUITimed=false;

static std::wstring gameProcessSelectionText(HWND control){
 if(!control)return {};
 const int size=std::min(GetWindowTextLengthW(control),32767);
 std::wstring text(size_t(std::max(0,size))+1,L'\0');
 const int copied=GetWindowTextW(control,text.data(),int(text.size()));
 text.resize(size_t(std::max(0,copied)));return text;
}
static std::wstring gameProcessSelectionTrim(const std::wstring& text){
 size_t begin=0,end=text.size();
 while(begin<end&&iswspace(text[begin]))++begin;
 while(end>begin&&iswspace(text[end-1]))--end;
 return text.substr(begin,end-begin);
}
static std::wstring gameProcessSelectionShort(const std::wstring& text,size_t limit){
 if(text.size()<=limit)return text;
 size_t prefix=(limit-1)/2,suffix=limit-1-prefix;
 // Keep complete UTF-16 surrogate pairs on Windows at both ellipsis edges.
 if(prefix&&text[prefix-1]>=0xD800&&text[prefix-1]<=0xDBFF)--prefix;
 if(suffix&&text[text.size()-suffix]>=0xDC00&&text[text.size()-suffix]<=0xDFFF)--suffix;
 return text.substr(0,prefix)+L"…"+text.substr(text.size()-suffix);
}
static void gameProcessSelectionMessage(const std::wstring& text,DWORD error=0){
 const auto next=text+(error?L"（Win32="+std::to_wstring(error)+L"）":L"");
 if(gameProcessSelectionClosing||next==gameProcessSelectionFeedback)return;
 gameProcessSelectionFeedback=next;
 if(gameProcessFeedbackText)SetWindowTextW(gameProcessFeedbackText,next.c_str());
}
static const external::targetselection::ProcessInfo* gameProcessSelectionSelected(){
 const auto at=SendMessageW(gameProcessRunningList,CB_GETCURSEL,0,0);
 if(at<0||size_t(at)>=gameProcessVisible.size())return nullptr;
 const size_t index=gameProcessVisible[size_t(at)];
 return index<gameProcessCandidates.size()?&gameProcessCandidates[index]:nullptr;
}
static void gameProcessSelectionFilter(){
 if(gameProcessSelectionClosing||!gameProcessRunningList)return;
 const auto* prior=gameProcessSelectionSelected();
 const DWORD previousPid=prior?prior->pid:0;
 const uint64_t previousCreation=prior?prior->creation:0;
 const auto query=gameProcessSelectionTrim(gameProcessSelectionText(gameProcessSearchEdit));
 gameProcessSelectionUpdating=true;gameProcessVisible.clear();
 SendMessageW(gameProcessRunningList,CB_RESETCONTENT,0,0);int selected=-1;
 for(size_t i=0;i<gameProcessCandidates.size();++i){
  const auto& process=gameProcessCandidates[i];
  const auto searchable=process.name+L" "+process.path+L" "+process.description;
  if(!query.empty()&&!external::targetselection::matchesKeywords(searchable,query))continue;
  std::wstring label=gameProcessSelectionShort(process.name,260)+L" · PID "+std::to_wstring(process.pid);
  if(!process.description.empty())label+=L" · "+gameProcessSelectionShort(process.description,256);
  if(!process.path.empty())label+=L" · "+gameProcessSelectionShort(process.path,300);
  if(process.identityError)label+=L" · 身份查询失败（"+std::to_wstring(process.identityError)+L"）";
  const auto row=SendMessageW(gameProcessRunningList,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
  if(row<0)continue;
  gameProcessVisible.push_back(i);
  SendMessageW(gameProcessRunningList,CB_SETITEMDATA,WPARAM(row),LPARAM(process.pid));
  if(process.pid==previousPid&&process.creation==previousCreation)selected=int(row);
 }
 if(selected<0&&gameProcessVisible.size()==1)selected=0;
 SendMessageW(gameProcessRunningList,CB_SETCURSEL,WPARAM(selected),0);
 gameProcessSelectionUpdating=false;
 EnableWindow(gameProcessRememberButton,gameProcessSelectionSelected()!=nullptr);
}
static DWORD WINAPI gameProcessSelectionWorker(void* parameter){
 std::unique_ptr<std::shared_ptr<GameProcessSelectionJob>> owner(static_cast<std::shared_ptr<GameProcessSelectionJob>*>(parameter));
 const auto job=*owner;owner.reset();
 try{job->processes=external::targetselection::enumerateProcesses(&job->error);}
 catch(...){job->error=ERROR_NOT_ENOUGH_MEMORY;job->processes.clear();}
 job->done.store(true,std::memory_order_release);return 0;
}
static void gameProcessSelectionRefresh(){
 if(gameProcessSelectionClosing)return;
 if(gameProcessSelectionJob&&!gameProcessSelectionJob->done.load(std::memory_order_acquire))return;
 auto job=std::make_shared<GameProcessSelectionJob>();job->generation=gameProcessSelectionGeneration+1;
 auto* context=new(std::nothrow) std::shared_ptr<GameProcessSelectionJob>(job);
 if(!context){gameProcessSelectionMessage(L"无法创建进程列表刷新任务。",ERROR_NOT_ENOUGH_MEMORY);return;}
 HANDLE worker=CreateThread(nullptr,0,gameProcessSelectionWorker,context,0,nullptr);
 if(!worker){const DWORD error=GetLastError();delete context;gameProcessSelectionMessage(L"无法刷新进程列表。",error);return;}
 CloseHandle(worker);gameProcessSelectionGeneration=job->generation;gameProcessSelectionJob=std::move(job);
 EnableWindow(gameProcessRefreshButton,FALSE);
 gameProcessSelectionMessage(L"正在读取运行进程。输入关键词可筛选列表；留空显示全部。");
}
static void updateGameProcessSelectionUI(bool force=false){
 if(gameProcessSelectionClosing||!gameProcessConnectionText||!IsWindow(gameProcessConnectionText))return;
 const DWORD now=GetTickCount();
 if(!force&&gameProcessSelectionUITimed&&DWORD(now-gameProcessSelectionUIAt)<250)return;
 gameProcessSelectionUIAt=now;gameProcessSelectionUITimed=true;
 const auto job=gameProcessSelectionJob;
 if(job&&job->done.load(std::memory_order_acquire)&&job->generation==gameProcessSelectionGeneration&&gameProcessSelectionPublished!=job->generation){
  gameProcessSelectionPublished=job->generation;
  if(job->error)gameProcessSelectionMessage(L"读取运行进程失败；保留上一次列表，请重试。",job->error);
  else{
   // Clear the old index before replacing its backing vector; a recycled PID
   // or a reordered snapshot must never silently change a pending selection.
   gameProcessVisible.clear();SendMessageW(gameProcessRunningList,CB_SETCURSEL,WPARAM(-1),0);
   gameProcessCandidates=job->processes;gameProcessSelectionFilter();
   gameProcessSelectionMessage(L"已读取 "+std::to_wstring(gameProcessCandidates.size())+L" 个运行进程；匹配 "+std::to_wstring(gameProcessVisible.size())+L" 个。选择后点击“连接并加入检测名单”。");
  }
 }
 EnableWindow(gameProcessRefreshButton,!job||job->done.load(std::memory_order_acquire));
 const auto policy=external::targetselection::current();
 if(policy.revision!=gameProcessSelectionPolicyRevision){
  const auto prior=SendMessageW(gameProcessRememberedList,CB_GETCURSEL,0,0);
  const std::wstring previous=prior>=0&&size_t(prior)<gameProcessRememberedPrograms.size()?gameProcessRememberedPrograms[size_t(prior)]:L"";
  gameProcessRememberedPrograms.clear();
  SendMessageW(gameProcessRememberedList,CB_RESETCONTENT,0,0);int selected=-1;
  for(size_t i=0;i<policy.rememberedPrograms.size();++i){
   const auto& program=policy.rememberedPrograms[i];
   const auto label=gameProcessSelectionShort(program,360);
   const auto row=SendMessageW(gameProcessRememberedList,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
   if(row>=0){gameProcessRememberedPrograms.push_back(program);SendMessageW(gameProcessRememberedList,CB_SETITEMDATA,WPARAM(row),LPARAM(i));if(program==previous)selected=int(row);}
  }
  if(selected<0&&gameProcessRememberedPrograms.size()==1)selected=0;
  SendMessageW(gameProcessRememberedList,CB_SETCURSEL,WPARAM(selected),0);
  gameProcessSelectionPolicyRevision=policy.revision;
 }
 const auto remembered=SendMessageW(gameProcessRememberedList,CB_GETCURSEL,0,0);
 EnableWindow(gameProcessRemoveButton,remembered>=0&&size_t(remembered)<gameProcessRememberedPrograms.size());
 EnableWindow(gameProcessClearButton,policy.selectedPid!=0);
 std::wstring connection=L"检测名单："+std::to_wstring(policy.rememberedPrograms.size())+L" 项；"+
  (policy.selectedPid?L"本次优先 PID "+std::to_wstring(policy.selectedPid):L"自动匹配全部检测规则");
 connection+=L"\r\n当前连接："+(pid?L"PID "+std::to_wstring(pid):std::wstring(L"尚未绑定游戏进程"));
 if(!enabled)connection+=L" · 战术控制已暂停";
 else if(pid){
  if(!shared)connection+=L" · 只读连接";
  else if(incoming.status<0)connection+=L" · 操作连接异常";
  else if(!lastData)connection+=L" · 等待游戏内回调";
  else if(DWORD(now-lastData)>=1500)connection+=L" · 等待游戏数据更新";
  else if(incoming.commandStatus==1)connection+=L" · 操作连接就绪";
  else connection+=L" · 等待对局可操作";
 }
 if(!statusText.empty())connection+=L"\r\n"+statusText;
 if(connection!=gameProcessSelectionConnection){gameProcessSelectionConnection=connection;SetWindowTextW(gameProcessConnectionText,connection.c_str());}
}
static bool gameProcessSelectionCommand(int id,unsigned notification){
 if(id<GameProcessSearch||id>GameProcessFeedback)return false;
 if(gameProcessSelectionClosing||gameProcessSelectionUpdating)return true;
 if(id==GameProcessSearch){if(notification==EN_CHANGE){gameProcessSelectionFilter();gameProcessSelectionMessage(L"当前匹配 "+std::to_wstring(gameProcessVisible.size())+L" 个进程。点击“保存搜索词”后，关键词会参与自动识别。");}return true;}
 if(id==GameProcessRunning){if(notification==CBN_SELCHANGE)EnableWindow(gameProcessRememberButton,gameProcessSelectionSelected()!=nullptr);return true;}
 if(id==GameProcessRemembered){if(notification==CBN_SELCHANGE)updateGameProcessSelectionUI(true);return true;}
 if(id==GameProcessTarget||id==GameProcessConnection||id==GameProcessFeedback||notification!=BN_CLICKED)return true;
 if(id==GameProcessRefresh){gameProcessSelectionRefresh();return true;}
 DWORD error=0;bool saved=false;
 if(id==GameProcessSaveSearch){
  saved=external::targetselection::saveKeywords(gameProcessSelectionTrim(gameProcessSelectionText(gameProcessSearchEdit)),&error);
  gameProcessSelectionMessage(saved?L"已保存搜索关键词。关键词、已记名单和内置识别规则共同参与检测。":L"关键词未能保存。",error);
 }else if(id==GameProcessRemember){
  const auto* selected=gameProcessSelectionSelected();
  if(!selected){gameProcessSelectionMessage(L"请先从运行进程列表选择一个程序。");return true;}
  saved=external::targetselection::rememberProcess(*selected,&error);
  gameProcessSelectionMessage(saved?L"已加入检测名单，并请求连接所选进程。其他游戏仍按原有规则检测。":L"未能记住此进程；它可能已退出，或无法读取身份。请刷新列表后重试。",error);
 }else if(id==GameProcessAdd){
  const auto target=gameProcessSelectionTrim(gameProcessSelectionText(gameProcessTargetEdit));
  if(target.empty()){gameProcessSelectionMessage(L"请输入完整程序路径或精确文件名，例如 gameares.exe。");return true;}
  saved=external::targetselection::addProgram(target,&error);
  gameProcessSelectionMessage(saved?L"已加入检测名单，下次启动仍会识别此程序。":L"未能添加记录，请检查程序路径或文件名。",error);
 }else if(id==GameProcessRemove){
  const auto row=SendMessageW(gameProcessRememberedList,CB_GETCURSEL,0,0);
  if(row<0||size_t(row)>=gameProcessRememberedPrograms.size()){gameProcessSelectionMessage(L"请先选择要移除的名单记录。");return true;}
  // Policy may have changed after the displayed list. Resolve the same saved
  // string again instead of deleting whatever now occupies its old index.
  const auto value=gameProcessRememberedPrograms[size_t(row)];const auto policy=external::targetselection::current();
  const auto found=std::find(policy.rememberedPrograms.begin(),policy.rememberedPrograms.end(),value);
  if(found==policy.rememberedPrograms.end()){gameProcessSelectionMessage(L"该记录已发生变化，请重新选择。");updateGameProcessSelectionUI(true);return true;}
  saved=external::targetselection::removeProgram(size_t(found-policy.rememberedPrograms.begin()),&error);
  gameProcessSelectionMessage(saved?L"已移除所选检测记录；内置识别与其他记录保持有效。":L"未能移除所选记录。",error);
 }else if(id==GameProcessClearPreferred){
  saved=external::targetselection::clearPreferredProcess(&error);
  gameProcessSelectionMessage(saved?L"已清除本次进程指定，继续按全部检测规则选择；检测名单已保留。":L"未能清除本次进程指定。",error);
 }
 if(saved)critical(L"game-process-policy action="+std::to_wstring(id)+L" revision="+std::to_wstring(external::targetselection::revision()));
 updateGameProcessSelectionUI(true);return true;
}
static void shutdownGameProcessSelectionUI(){
 gameProcessSelectionClosing=true;++gameProcessSelectionGeneration;gameProcessSelectionJob.reset();
 // The worker owns its result and only reads process metadata. Never wait for
 // it during shutdown, and never let a late completion address a HWND.
}
static int createGameProcessSelectionControls(int page,int y){
 gameProcessSelectionClosing=false;gameProcessSelectionUITimed=false;gameProcessSelectionPolicyRevision=~uint64_t(0);
 const auto policy=external::targetselection::current();
 auto add=[&](const wchar_t* cls,const wchar_t* text,int x,int row,int width,int height,int id,DWORD style=0,const wchar_t* help=L""){
  HWND control=pageControl(page,cls,text,x,row,width,height,id,style);
  pageControls.back().help=help;if(*help)modernui::tip(control,help);
  SetPropW(control,L"RA2.Console.Group",const_cast<wchar_t*>(L"游戏进程连接"));
  SetPropW(control,L"RA2.Console.Collapsible",reinterpret_cast<HANDLE>(1));
  // Keep this action group initially open so the chosen process stays visible.
  // ActionRole: 1 = primary, 2 = destructive; unmarked actions stay secondary.
  if(id==GameProcessRemember)SetPropW(control,L"RA2.Console.ActionRole",reinterpret_cast<HANDLE>(1));
  else if(id==GameProcessRemove)SetPropW(control,L"RA2.Console.ActionRole",reinterpret_cast<HANDLE>(2));
  SetPropW(control,L"RA2.Console.Row",reinterpret_cast<HANDLE>(INT_PTR(row)+1));
  SetPropW(control,L"RA2.Console.Column",reinterpret_cast<HANDLE>(INT_PTR(x)+1));
  return control;
 };
 modernui::heading(add(L"STATIC",L"游戏进程连接",20,y,610,26,0));y+=34;
 add(L"STATIC",L"搜索程序并加入检测名单；原有游戏识别规则继续有效。",20,y,610,30,0);y+=38;
 add(L"STATIC",L"模糊关键词",20,y,130,26,0);
 gameProcessSearchEdit=add(L"EDIT",policy.keywords.c_str(),160,y,284,28,GameProcessSearch,WS_TABSTOP|ES_AUTOHSCROLL,
  L"按程序名称、完整路径和描述筛选。多个关键词用空格分隔；留空显示全部运行进程。保存后参与自动识别，不排除已记名单或内置游戏。");
 add(L"BUTTON",L"保存搜索词",456,y,166,30,GameProcessSaveSearch,WS_TABSTOP);y+=38;
 add(L"STATIC",L"运行进程",20,y,130,26,0);
 gameProcessRunningList=add(L"COMBOBOX",L"",160,y,284,180,GameProcessRunning,WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL);
 gameProcessRefreshButton=add(L"BUTTON",L"刷新列表",456,y,166,30,GameProcessRefresh,WS_TABSTOP);y+=38;
 gameProcessRememberButton=add(L"BUTTON",L"连接并加入检测名单",160,y,284,32,GameProcessRemember,WS_TABSTOP,
  L"点击时重新核验进程身份，记住程序路径并优先连接当前实例。PID 仅用于本次会话；下次启动继续扫描全部规则。");y+=40;
 add(L"STATIC",L"路径或精确文件名",20,y,180,26,0);y+=30;
 gameProcessTargetEdit=add(L"EDIT",L"",20,y,424,28,GameProcessTarget,WS_TABSTOP|ES_AUTOHSCROLL,
  L"可填写完整程序路径，或精确文件名，例如 gameares.exe。记录追加到检测名单，不替换其他游戏。");
 add(L"BUTTON",L"添加路径或名称",456,y,166,30,GameProcessAdd,WS_TABSTOP);y+=38;
 add(L"STATIC",L"已记住的程序",20,y,130,26,0);
 gameProcessRememberedList=add(L"COMBOBOX",L"",160,y,284,180,GameProcessRemembered,WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL);
 gameProcessRemoveButton=add(L"BUTTON",L"移除所选记录",456,y,166,30,GameProcessRemove,WS_TABSTOP);y+=38;
 gameProcessClearButton=add(L"BUTTON",L"清除本次指定",160,y,284,30,GameProcessClearPreferred,WS_TABSTOP,
  L"只取消本次指定的进程优先级，保留所有已记检测记录和关键词。");y+=40;
 gameProcessConnectionText=add(L"STATIC",L"",20,y,610,94,GameProcessConnection);y+=102;
 gameProcessFeedbackText=add(L"STATIC",L"",20,y,610,66,GameProcessFeedback);y+=78;
 SendMessageW(gameProcessSearchEdit,EM_SETLIMITTEXT,512,0);
 SendMessageW(gameProcessTargetEdit,EM_SETLIMITTEXT,32760,0);
 EnableWindow(gameProcessRememberButton,FALSE);
 updateGameProcessSelectionUI(true);gameProcessSelectionRefresh();return y;
}
