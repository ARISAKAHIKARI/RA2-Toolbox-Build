#pragma once
#include "native_platform_profile.h"
#include "platform_hook_probe_ui.h"
// Included by the Host after pageControl() and settings persistence. The
// renderer uses ordinary adapter-issued control tokens, never raw addresses.
static constexpr int DebugEntryControlBase=12000;
static HWND debugEntryChecks[debugentries::Capacity]{},debugEntryDetails[debugentries::Capacity]{};
static std::wstring debugEntryGroups[debugentries::Capacity];
#ifdef RA2_CONSOLE_WEBVIEW2
static consolemodel::DebugEntry debugEntryMetadata[debugentries::Capacity];
#endif
static std::wstring debugWide(const char*text){
 if(!text||!*text)return {};
 const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,nullptr,0);if(n<=1)return {};
 std::wstring out(size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,out.data(),n);out.resize(size_t(n-1));return out;
}
static const char*debugKind(debugentries::Kind kind){
 using debugentries::Kind;return kind==Kind::Call?"call":kind==Kind::Patch?"patch":kind==Kind::Breakpoint?"breakpoint":"system";
}
static const char*debugHook(debugentries::Hook hook){
 using debugentries::Hook;switch(hook){case Hook::NA:return "na";case Hook::Inactive:return "inactive";case Hook::Pending:return "pending";
 case Hook::Installed:return "installed";case Hook::Conflict:return "conflict";case Hook::Unavailable:return "unknown";}return "unknown";
}
static const char*debugReason(debugentries::Reason reason){
 using debugentries::Reason;switch(reason){
 case Reason::None:return "";case Reason::Signature:return "参考签名不匹配";case Reason::Unreadable:return "入口内存不可读或不可执行";
 case Reason::Foreign:return "入口归属不匹配或现场被其他写入修改";case Reason::NoRequest:return "相关功能未请求此入口";
 case Reason::OwnerPending:return "等待确认安装或清理本工具持有的入口";case Reason::Unsupported:return "当前实现不支持此入口";
 case Reason::Disabled:return "本版本没有启用此入口";case Reason::PathUnreachable:return "当前平台的执行路径不经过此入口";
 case Reason::Structural:return "入口指令结构或原目标不支持当前适配器";case Reason::AwaitRuntime:return "等待运行组件采样";
 }return "";
}
static const char*debugPlatformFailure(int error){
 using nativeplatform::Failure;
 if(error>=-nativeplatform::ErrorBase||error<-nativeplatform::ErrorBase-int(Failure::Changed))return "";
 switch(Failure(-error-nativeplatform::ErrorBase)){
 case Failure::NotProfile:return "该调用没有已知平台适配";
 case Failure::EntryUnreadable:return "无法读取平台入口";
 case Failure::EntryLayout:return "平台入口未改写部分不符合已知布局";
 case Failure::SiteJump:return "平台跳转位置或覆盖长度不符合适配";
 case Failure::StubUnreadable:return "平台跳板不可读或不可执行";
 case Failure::StubShape:return "加载器跳板或调用约定不符合已知结构";
 case Failure::UnknownCallbackChain:return "同址额外回调链尚未适配";
 case Failure::Replay:return "平台跳板搬移的原指令不匹配";
 case Failure::Continuation:return "平台跳板续接位置不匹配";
 case Failure::ModuleIdentity:return "平台回调不属于已加载映像";
 case Failure::ModuleLayout:return "平台模块结构不符合适配";
 case Failure::HookDeclaration:return "模块没有匹配的入口声明";
 case Failure::Export:return "模块没有匹配的具名导出";
 case Failure::Capabilities:return "加载器能力标记不满足适配";
 case Failure::CallbackOwner:return "回调目标与声明模块或导出不一致";
 case Failure::CallbackShape:return "平台回调本身存在尚未适配的改写";
 case Failure::Companion:return "CanBuild 配套入口不能确认归属，暂缓调用";
 case Failure::Changed:return "检查期间入口发生变化，等待重新检查";
 case Failure::None:return "";
 }return "";
}
static const char*debugSignature(debugentries::Signature signature){
 using debugentries::Signature;switch(signature){case Signature::Match:return "签名匹配";case Signature::Platform:return "已识别的平台兼容入口";
 case Signature::Mismatch:return "签名不匹配";case Signature::Skipped:return "签名校验已跳过";case Signature::Unreadable:return "入口不可读或不可执行";
 case Signature::Unknown:return "尚未检查签名";}return "尚未检查签名";
}
static void createDebugEntryControls(){
 const int page=suite::categories::Debug;int y=createPlatformHookProbeControls(page,uilayout::BodyTop+12);
 pageControl(page,L"STATIC",L"入口调试：用途、影响特性与实时安装状态",20,y,610,26,0);y+=34;
#ifdef RA2_CONSOLE_WEBVIEW2
 SetPropW(pageControls.back().hwnd,L"RA2.Console.Skip",reinterpret_cast<HANDLE>(1));
#endif
 pageControl(page,L"STATIC",L"每项独立保存。关闭校验不启用对应功能；调用、补丁、断点各自控制。",20,y,610,40,0);y+=48;
#ifdef RA2_CONSOLE_WEBVIEW2
 SetPropW(pageControls.back().hwnd,L"RA2.Console.Skip",reinterpret_cast<HANDLE>(1));
#endif
 for(uint32_t i=0;i<debugentries::Count;++i){const auto&e=debugentries::Entries[i];
  wchar_t address[32]{};swprintf(address,32,L" · 0x%08X",unsigned(e.address));
  const std::wstring label=debugWide(e.name)
#ifndef RA2_CONSOLE_WEBVIEW2
+address
#endif
;
  HWND c=debugEntryChecks[i]=pageControl(page,L"BUTTON",label.c_str(),20,y,610,28,DebugEntryControlBase+int(i),BS_AUTOCHECKBOX|WS_TABSTOP);
  SendMessageW(c,BM_SETCHECK,debugentries::required(extra.debugPolicy,e.id)?BST_CHECKED:BST_UNCHECKED,0);
  EnableWindow(c,(e.flags&debugentries::Checkable)!=0);
  debugEntryGroups[i]=debugWide(e.group);
  const std::wstring help=debugWide(e.purpose)+L"\r\n影响特性："+debugWide(e.features)+L"\r\n关闭后仅跳过此入口的参考签名；线程、内存、原目标结构与恢复记录继续检查。";
  pageControls.back().help=help;modernui::tip(c,help);
#ifdef RA2_CONSOLE_WEBVIEW2
  auto&m=debugEntryMetadata[i];m.present=true;m.siteId=e.id;m.address=e.address;m.kind=debugKind(e.kind);m.purpose=e.purpose;m.features=e.features;
  m.state="unknown";m.hook=e.kind==debugentries::Kind::Call?"na":"unknown";m.detail="等待运行组件采样";
  SetPropW(c,L"RA2.Console.DebugEntry",reinterpret_cast<HANDLE>(&m));
  SetPropW(c,L"RA2.Console.Group",reinterpret_cast<HANDLE>(debugEntryGroups[i].data()));
  SetPropW(c,L"RA2.Console.Row",reinterpret_cast<HANDLE>(INT_PTR(y)+1));
  SetPropW(c,L"RA2.Console.Column",reinterpret_cast<HANDLE>(21));
  y+=40;
#else
  y+=30;debugEntryDetails[i]=pageControl(page,L"STATIC",help.c_str(),38,y,590,80,0);y+=90;
#endif
 }
 for(auto&c:pageControls)if(c.page==page)c.fullRect=c.rect;
}
static void updateDebugEntries(bool force){
 updatePlatformHookProbeUI();
 if(!force&&!debugPageActive)return;
 static DWORD lastAt=0;const DWORD now=GetTickCount();if(!force&&lastAt&&DWORD(now-lastAt)<250)return;lastAt=now;
 if(!ui||!debugEntryChecks[0])return;
 debugentries::Snapshot sampled{};bool valid=false,catalogMismatch=false;
 if(shared&&shared->magic==Magic&&shared->version==Version&&shared->pid==pid){
  shared->debugVisible=debugPageActive?1u:0u;
  for(unsigned attempt=0;attempt<3;++attempt){const LONG before=InterlockedCompareExchange(&shared->debugSnapshotSeq,0,0);if(before&1)continue;
   memcpy(&sampled,&shared->debugSnapshot,sizeof sampled);MemoryBarrier();
   if(before==InterlockedCompareExchange(&shared->debugSnapshotSeq,0,0)){
    valid=sampled.tick&&sampled.count==debugentries::Count;
    catalogMismatch=sampled.tick&&sampled.count!=debugentries::Count;
    if(valid)for(uint32_t i=0;i<debugentries::Count;++i)if(sampled.entries[i].id!=debugentries::Entries[i].id){valid=false;catalogMismatch=true;break;}
    break;
   }
  }
 }
 for(uint32_t i=0;i<debugentries::Count;++i){const auto&e=debugentries::Entries[i];const auto&o=sampled.entries[i];
  const bool observed=valid&&o.id==e.id&&o.tick!=0,fresh=observed&&DWORD(now-o.tick)<=2000;
  const bool applied=valid&&sampled.policyGeneration==extra.debugPolicy.generation;
  std::string state="unknown",hook=e.kind==debugentries::Kind::Call?"na":"unknown",detail;
  if(e.flags&debugentries::Disabled){state="unused";detail="本版本没有启用此入口";}
  else if(!valid)detail=catalogMismatch?"运行组件入口目录不一致，请成套更新工具箱":shared?"等待运行组件采样":"尚未连接运行组件；设置已保存在本机";
  else{
   hook=debugHook(o.hook);
   if(!observed)detail=debugReason(o.reason);
   else{
    detail=(e.flags&debugentries::Checkable)?debugSignature(o.signature):"无需机器码签名校验";
    if((e.flags&debugentries::Checkable)&&o.hook==debugentries::Hook::Installed&&o.signature==debugentries::Signature::Unknown)
     detail="安装状态已确认；签名设置等待下次准入检查";
    const char*reason=debugReason(o.reason);if(*reason&&detail!=reason)detail+=std::string("；")+reason;
    const bool blocked=o.signature==debugentries::Signature::Mismatch||o.signature==debugentries::Signature::Unreadable||
     o.hook==debugentries::Hook::Conflict||o.hook==debugentries::Hook::Unavailable||
     o.reason==debugentries::Reason::Structural||o.reason==debugentries::Reason::PathUnreachable;
    if(fresh)state=blocked?"blocked":o.hook==debugentries::Hook::Installed||
     o.signature==debugentries::Signature::Match||o.signature==debugentries::Signature::Platform||
     (e.kind==debugentries::Kind::Call&&o.signature==debugentries::Signature::Skipped)?"ready":"unknown";
    if(o.error){const char*platform=debugPlatformFailure(o.error);if(*platform)detail+=std::string("；")+platform;
     if(e.kind==debugentries::Kind::Breakpoint&&o.error==ERROR_NOT_SUPPORTED)detail+="；硬件断点上下文或实际命中未通过确认";
     detail+="；错误码 "+std::to_string(o.error);}
    if(!fresh){detail+="；采样已过期，当前状态待确认";if(hook!="na")hook="unknown";}
   }
   if(!applied){detail+="；新设置等待运行组件应用";state="unknown";}
  }
  const bool required=debugentries::required(extra.debugPolicy,e.id);
  if(!required&&(!observed||o.signature!=debugentries::Signature::Skipped))detail+="；此项签名校验已关闭";
#ifdef RA2_CONSOLE_WEBVIEW2
  auto&m=debugEntryMetadata[i];m.state=state;m.hook=hook;m.detail=detail;m.sampled=observed;m.ageMs=observed?DWORD(now-o.tick):0;
#else
  const std::wstring text=debugWide(e.purpose)+L"\r\n影响："+debugWide(e.features)+L"\r\n"+debugWide(detail.c_str());
  if(debugEntryDetails[i])SetWindowTextW(debugEntryDetails[i],text.c_str());
#endif
  const LRESULT wanted=required?BST_CHECKED:BST_UNCHECKED;
  if(SendMessageW(debugEntryChecks[i],BM_GETCHECK,0,0)!=wanted)SendMessageW(debugEntryChecks[i],BM_SETCHECK,wanted,0);
 }
}
static bool debugEntryCommand(int controlId,unsigned notification){
 if(platformHookProbeCommand(controlId,notification))return true;
 if(controlId<DebugEntryControlBase||controlId>=DebugEntryControlBase+int(debugentries::Count))return false;
 if(notification!=BN_CLICKED||!ready||updatingControls)return true;
 const uint32_t i=uint32_t(controlId-DebugEntryControlBase);const auto&e=debugentries::Entries[i];
 if(e.flags&debugentries::Checkable){
  const bool required=SendMessageW(debugEntryChecks[i],BM_GETCHECK,0,0)==BST_CHECKED;
  if(debugentries::setSkip(extra.debugPolicy,e.id,!required)){syncAdditional();changedSetting();wakeGame();}
 }
 updateDebugEntries(true);return true;
}
